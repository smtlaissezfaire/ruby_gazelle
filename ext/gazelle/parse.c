/*********************************************************************

  Gazelle: a system for building fast, reusable parsers

  interpreter.c

  Once a compiled grammar has been loaded into memory, the routines
  in this file are what actually does the parsing.  This file is an
  "interpreter" in the sense that it parses the input by using the
  grammar as a data structure -- no grammar-specific code is ever
  generated or executed.  Despite this, it is still quite fast, and
  has a very low memory footprint.

  The interpreter primarily consists of maintaining the parse stack
  properly and transitioning the frames in response to the input.

  Copyright (c) 2007-2009 Joshua Haberman.  See LICENSE for details.

*********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "gazelle/parse.h"

/*
 * The following are stack-manipulation functions.  Gazelle maintains a runtime
 * stack (which is completely separate from the C stack), and these functions
 * provide pushing and popping of different kinds of stack frames.
 */

static
struct gzl_parse_stack_frame *push_empty_frame(struct gzl_parse_state *s,
                                               enum gzl_frame_type frame_type,
                                               struct gzl_offset *start_offset)
{
    RESIZE_DYNARRAY(s->parse_stack, s->parse_stack_len+1);
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    frame->frame_type = frame_type;
    frame->start_offset = *start_offset;
    return frame;
}

static
struct gzl_intfa_frame *push_intfa_frame(struct gzl_parse_state *s,
                                         struct gzl_intfa *intfa,
                                         struct gzl_offset *start_offset)
{
    struct gzl_parse_stack_frame *frame =
        push_empty_frame(s, GZL_FRAME_TYPE_INTFA, start_offset);
    struct gzl_intfa_frame *intfa_frame = &frame->f.intfa_frame;
    intfa_frame->intfa        = intfa;
    intfa_frame->intfa_state  = &intfa->states[0];
    return intfa_frame;
}

static
struct gzl_parse_stack_frame *push_gla_frame(struct gzl_parse_state *s,
                                             struct gzl_gla *gla,
                                             struct gzl_offset *start_offset)
{
    struct gzl_parse_stack_frame *frame =
        push_empty_frame(s, GZL_FRAME_TYPE_GLA, start_offset);
    struct gzl_gla_frame *gla_frame = &frame->f.gla_frame;
    gla_frame->gla          = gla;
    gla_frame->gla_state    = &gla->states[0];
    return frame;
}

static
enum gzl_status push_rtn_frame(struct gzl_parse_state *s,
                               struct gzl_rtn *rtn,
                               struct gzl_offset *start_offset)
{
    struct gzl_parse_stack_frame *new_frame =
        push_empty_frame(s, GZL_FRAME_TYPE_RTN, start_offset);
    struct gzl_rtn_frame *new_rtn_frame = &new_frame->f.rtn_frame;
    new_rtn_frame->rtn            = rtn;
    new_rtn_frame->rtn_transition = NULL;
    new_rtn_frame->rtn_state      = &new_rtn_frame->rtn->states[0];
    if(s->bound_grammar->start_rule_cb) s->bound_grammar->start_rule_cb(s);
    return GZL_STATUS_OK;
}

static
enum gzl_status push_rtn_frame_for_transition(struct gzl_parse_state *s,
                                              struct gzl_rtn_transition *t,
                                              struct gzl_offset *start_offset)
{
    struct gzl_rtn_frame *old_rtn_frame =
        &DYNARRAY_GET_TOP(s->parse_stack)->f.rtn_frame;
    old_rtn_frame->rtn_transition = t;
    return push_rtn_frame(s, t->edge.nonterminal, start_offset);
}

static
struct gzl_parse_stack_frame *pop_frame(struct gzl_parse_state *s)
{
    assert(s->parse_stack_len > 0);
    RESIZE_DYNARRAY(s->parse_stack, s->parse_stack_len-1);
    return s->parse_stack_len > 0 ? DYNARRAY_GET_TOP(s->parse_stack) : NULL;
}

static
enum gzl_status pop_rtn_frame(struct gzl_parse_state *s)
{
    assert(DYNARRAY_GET_TOP(s->parse_stack)->frame_type == GZL_FRAME_TYPE_RTN);
    if(s->bound_grammar->end_rule_cb) s->bound_grammar->end_rule_cb(s);

    struct gzl_parse_stack_frame *frame = pop_frame(s);
    if(frame) {
        assert(frame->frame_type == GZL_FRAME_TYPE_RTN);
        struct gzl_rtn_frame *rtn_frame = &frame->f.rtn_frame;
        if(rtn_frame->rtn_transition)
            rtn_frame->rtn_state = rtn_frame->rtn_transition->dest_state;
        else {
          /* Should only happen at the top level. */
          assert(s->parse_stack_len == 1);
        }
        return GZL_STATUS_OK;
    } else
        return GZL_STATUS_HARD_EOF;
}

static
struct gzl_parse_stack_frame *pop_gla_frame(struct gzl_parse_state *s)
{
    assert(DYNARRAY_GET_TOP(s->parse_stack)->frame_type == GZL_FRAME_TYPE_GLA);
    return pop_frame(s);
}

static
struct gzl_parse_stack_frame *pop_intfa_frame(struct gzl_parse_state *s)
{
    assert(DYNARRAY_GET_TOP(s->parse_stack)->frame_type ==
           GZL_FRAME_TYPE_INTFA);
    return pop_frame(s);
}

/*
 * descend_to_gla(): given the current parse stack, pushes any RTN or GLA
 * stack frames representing transitions that can be taken without consuming
 * any terminals.
 *
 * Preconditions:
 * - the current frame is either an RTN frame or a GLA frame
 *
 * Postconditions:
 * - the current frame is an RTN frame or a GLA frame.  If a new GLA frame was
 *   entered, entered_gla is set to true.
 */
static
enum gzl_status descend_to_gla(struct gzl_parse_state *s, bool *entered_gla,
                               struct gzl_offset *start_offset)
{
    *entered_gla = false;
    while(true) {
        struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
        if(frame->frame_type != GZL_FRAME_TYPE_RTN) return GZL_STATUS_OK;

        /* Subtract 1 because there can be one IntFA frame beyond the RTN and
         * GLA frames this function pushes. */
        if(s->parse_stack_len >= s->max_stack_depth-1)
            return GZL_STATUS_RESOURCE_LIMIT_EXCEEDED;

        struct gzl_rtn_frame *rtn_frame = &frame->f.rtn_frame;
        switch(rtn_frame->rtn_state->lookahead_type) {
          case GZL_STATE_HAS_INTFA:
            return GZL_STATUS_OK;

          case GZL_STATE_HAS_GLA:
            *entered_gla = true;
            push_gla_frame(s, rtn_frame->rtn_state->d.state_gla, start_offset);
            return GZL_STATUS_OK;

          case GZL_STATE_HAS_NEITHER:
            /* An RTN state has neither an IntFA or a GLA in only two cases:
             * - it is a final state with no outgoing transitions
             * - it is a nonfinal state with only one transition (a nonterminal)
             */
            assert(rtn_frame->rtn_state->num_transitions < 2);
            enum gzl_status status = GZL_STATUS_OK;
            if(rtn_frame->rtn_state->num_transitions == 0)
                status = pop_rtn_frame(s); /* Final state */
            else if(rtn_frame->rtn_state->num_transitions == 1) {
                assert(rtn_frame->rtn_state->transitions[0].transition_type ==
                       GZL_NONTERM_TRANSITION);
                status = push_rtn_frame_for_transition(
                    s, &rtn_frame->rtn_state->transitions[0], start_offset);
            }
            if(status != GZL_STATUS_OK) return status;
            break;
        }
    }
}

static
struct gzl_intfa_frame *push_intfa_frame_for_gla_or_rtn(
    struct gzl_parse_state *s)
{
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    if(frame->frame_type == GZL_FRAME_TYPE_GLA) {
        struct gzl_gla_state *gla_state = frame->f.gla_frame.gla_state;
        assert(gla_state->is_final == false);
        return push_intfa_frame(s, gla_state->d.nonfinal.intfa, &s->offset);
    } else if(frame->frame_type == GZL_FRAME_TYPE_RTN) {
        struct gzl_rtn_state *rtn_state = frame->f.rtn_frame.rtn_state;
        assert(rtn_state->lookahead_type == GZL_STATE_HAS_INTFA);
        return push_intfa_frame(s, rtn_state->d.state_intfa, &s->offset);
    }
    assert(false);  /* should never reach here. */
    return NULL;
}

static
enum gzl_status do_rtn_terminal_transition(struct gzl_parse_state *s,
                                           struct gzl_rtn_transition *t,
                                           struct gzl_terminal *terminal)
{
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    assert(frame->frame_type == GZL_FRAME_TYPE_RTN);
    struct gzl_rtn_frame *rtn_frame = &frame->f.rtn_frame;
    rtn_frame->rtn_transition = t;
    if(s->bound_grammar->terminal_cb)
      s->bound_grammar->terminal_cb(s, terminal);
    assert(t->transition_type == GZL_TERMINAL_TRANSITION);
    rtn_frame->rtn_state = t->dest_state;
    return GZL_STATUS_OK;
}

static
struct gzl_rtn_transition *find_rtn_terminal_transition(
    struct gzl_rtn_state *rtn_state, struct gzl_terminal *terminal)
{
    int i;
    for(i = 0; i < rtn_state->num_transitions; i++) {
        struct gzl_rtn_transition *t = &rtn_state->transitions[i];
        if(t->transition_type == GZL_TERMINAL_TRANSITION &&
           t->edge.terminal_name == terminal->name)
            return t;
    }
    return NULL;
}

static
struct gzl_gla_transition *find_gla_transition(struct gzl_gla_state *gla_state,
                                               char *term_name)
{
    int i;
    for(i = 0; i < gla_state->d.nonfinal.num_transitions; i++) {
        struct gzl_gla_transition *t = &gla_state->d.nonfinal.transitions[i];
        if(t->term == term_name)
            return t;
    }
    return NULL;
}

static
struct gzl_intfa_transition *find_intfa_transition(
    struct gzl_intfa_state *intfa_state, char ch)
{
    int i;
    for(i = 0; i < intfa_state->num_transitions; i++) {
        struct gzl_intfa_transition *t = &intfa_state->transitions[i];
        if(ch >= t->ch_low && ch <= t->ch_high)
            return t;
    }
    return NULL;
}

/*
 * do_gla_transition(): transitions a GLA frame, performing the appropriate
 * RTN transitions if this puts the GLA in a final state.
 *
 * Preconditions:
 * - the current stack frame is a GLA frame
 * - term is a terminal that came from this GLA state's intfa
 *
 * Postconditions:
 * - the current stack frame is a GLA frame (this would indicate that
 *   the GLA hasn't hit a final state yet) or the current stack frame is
 *   an RTN frame (indicating we *have* hit a final state in the GLA)
 */
static
enum gzl_status do_gla_transition(struct gzl_parse_state *s,
                                        struct gzl_terminal *term,
                                        size_t *rtn_term_offset)
{
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    assert(frame->frame_type == GZL_FRAME_TYPE_GLA);
    assert(frame->f.gla_frame.gla_state->is_final == false);
    struct gzl_gla_state *gla_state = frame->f.gla_frame.gla_state;
    struct gzl_gla_state *dest_gla_state = NULL;

    /* Find the transition. */
    struct gzl_gla_transition *t = find_gla_transition(gla_state, term->name);
    if(!t) {
        /* Parse error: terminal for which we had no GLA transition. */
        if(s->bound_grammar->error_terminal_cb)
            s->bound_grammar->error_terminal_cb(s, term);
        return GZL_STATUS_ERROR;
    }
    /* Perform the transition. */
    assert(t->dest_state);
    frame->f.gla_frame.gla_state = t->dest_state;
    dest_gla_state = t->dest_state;

    /* Perform appropriate actions if we're in a final state. */
    enum gzl_status status = GZL_STATUS_OK;
    if(dest_gla_state->is_final) {
        /* Pop the GLA frame (since now we know what RTN transition to take)
         * and use its information to make an RTN transition. */
        int offset = dest_gla_state->d.final.transition_offset;
        frame = pop_gla_frame(s);
        if(offset == 0)
            status = pop_rtn_frame(s);
        else {
            struct gzl_rtn_state *rtn_state = frame->f.rtn_frame.rtn_state;
            struct gzl_rtn_transition *t = &rtn_state->transitions[offset-1];
            struct gzl_terminal *next_term = &s->token_buffer[*rtn_term_offset];
            if(t->transition_type == GZL_TERMINAL_TRANSITION) {
                /* The transition must match what we have in the token buffer */
                assert(next_term->name == t->edge.terminal_name);
                (*rtn_term_offset)++;
                status = do_rtn_terminal_transition(s, t, next_term);
            } else
                status = push_rtn_frame_for_transition(
                    s, t, &next_term->offset);
        }
    }
    return status;
}

/*
 * process_terminal(): processes a terminal that was just lexed, possibly
 * triggering a series of RTN and/or GLA transitions.
 *
 * Preconditions:
 * - the current stack frame is an intfa frame representing the intfa that
 *   just produced this terminal
 * - the given terminal can be recognized by the current GLA or RTN state
 *
 * Postconditions:
 * - the current stack frame is an GLA or RTN frame representing the state after
 *   all available GLA and RTN transitions have been taken.
 */

static
enum gzl_status process_terminal(struct gzl_parse_state *s, char *term_name,
                                 struct gzl_offset *start_offset, int len)
{
    pop_intfa_frame(s);
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    size_t rtn_term_offset = 0;
    size_t gla_term_offset = s->token_buffer_len;

    RESIZE_DYNARRAY(s->token_buffer, s->token_buffer_len+1);
    if(s->token_buffer_len >= s->max_lookahead)
        return GZL_STATUS_RESOURCE_LIMIT_EXCEEDED;

    struct gzl_terminal *term = DYNARRAY_GET_TOP(s->token_buffer);
    term->name = term_name;
    term->offset = *start_offset;
    term->len = len;

    /* Feed tokens to RTNs and GLAs until we have processed all the tokens we
     * have. */
    enum gzl_status status = GZL_STATUS_OK;
    enum gzl_frame_type frame_type = frame->frame_type;
    do {
        /* Take one terminal transition, for either an RTN or a GLA. */
        if(frame_type == GZL_FRAME_TYPE_RTN) {
            struct gzl_terminal *rtn_term = &s->token_buffer[rtn_term_offset];
            struct gzl_rtn_transition *t;
            rtn_term_offset++;

            if(rtn_term->name == NULL)
                /* Skip: RTNs don't process EOF as a terminal, only GLAs do. */
                continue;
            t = find_rtn_terminal_transition(frame->f.rtn_frame.rtn_state,
                                             rtn_term);
            if(!t) {
                /* Parse error: terminal for which we had no RTN transition. */
                if(s->bound_grammar->error_terminal_cb)
                    s->bound_grammar->error_terminal_cb(s, term);
                return GZL_STATUS_ERROR;
            }
            status = do_rtn_terminal_transition(s, t, rtn_term);
        } else {
            struct gzl_terminal *gla_term = &s->token_buffer[gla_term_offset++];
            status = do_gla_transition(s, gla_term, &rtn_term_offset);
        }

        /* Having taken a transition, push any new frames onto the stack. */
        if(status == GZL_STATUS_OK) {
            bool entered_gla;
            if(rtn_term_offset < s->token_buffer_len)
                status = descend_to_gla(
                    s, &entered_gla, &s->token_buffer[rtn_term_offset].offset);
            else
                status = descend_to_gla(s, &entered_gla, &s->offset);

            if(entered_gla)
                gla_term_offset = rtn_term_offset;
        }

        if(status == GZL_STATUS_OK) {
            assert(s->parse_stack_len > 0);
            frame = DYNARRAY_GET_TOP(s->parse_stack);
            frame_type = frame->frame_type;
        }
    }
    while(status == GZL_STATUS_OK &&
          ((frame_type == GZL_FRAME_TYPE_RTN &&
            rtn_term_offset < s->token_buffer_len) ||
           (frame_type == GZL_FRAME_TYPE_GLA &&
            gla_term_offset < s->token_buffer_len)));

    /* We can have an EOF left over in the token buffer if the EOF token led us
     * to a hard EOF, thus terminating the above loop before our "skip" above
     * could cover this EOF special case. */
    if(rtn_term_offset < s->token_buffer_len &&
       s->token_buffer[rtn_term_offset].name == NULL)
        rtn_term_offset++;

    /* At this point we have consumed some (but possibly not all) of the
     * terminals we have lexed.  We consider a token fully consumed when it
     * has caused an RTN transition (just a GLA transition doesn't leave the
     * token consumed, because it will be used again for an RTN transition
     * later.
     *
     * We now remove the consumed terminals from token_buffer. */
    size_t remaining_terminals = s->token_buffer_len - rtn_term_offset;
    if(remaining_terminals > 0)
        memmove(s->token_buffer, s->token_buffer + rtn_term_offset,
                remaining_terminals * sizeof(*s->token_buffer));
    RESIZE_DYNARRAY(s->token_buffer, remaining_terminals);

    /* Update open_terminal_offset. */
    if(remaining_terminals > 0)
        s->open_terminal_offset = s->token_buffer[0].offset;
    else
        s->open_terminal_offset = s->offset;

    return status;
}


/*
 * do_intfa_transition(): transitions an IntFA frame according to the given
 * char, performing the appropriate GLA/RTN transitions if this puts the IntFA
 * in a final state.
 *
 * Preconditions:
 * - the current stack frame is an IntFA frame
 *
 * Postconditions:
 * - the current stack frame is an IntFA frame unless we have hit a
 *   hard EOF in which case it is an RTN frame.  Note that it could be either
 *   same IntFA frame or a different one.
 *
 * Note: we currently implement longest-match, assuming that the first
 * non-matching character is only one longer than the longest match.
 */
static
enum gzl_status do_intfa_transition(struct gzl_parse_state *s,
                                    char ch)
{
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    assert(frame->frame_type == GZL_FRAME_TYPE_INTFA);
    struct gzl_intfa_frame *intfa_frame = &frame->f.intfa_frame;
    struct gzl_intfa_transition *t = find_intfa_transition(
        intfa_frame->intfa_state, ch);
    enum gzl_status status;

    /* If this character did not have any transition, but the state we're coming
     * from is final, then longest-match semantics say that we should return
     * the last character's final state as the token.  But if the state we're
     * coming from is *not* final, it's just a parse error. */
    if(!t) {
        char *terminal = intfa_frame->intfa_state->final;
        assert(terminal);  /* TODO: handle this case. */
        status = process_terminal(s, terminal, &frame->start_offset,
                                  s->offset.byte - frame->start_offset.byte);
        if(status != GZL_STATUS_OK) return status;
        intfa_frame = push_intfa_frame_for_gla_or_rtn(s);
        t = find_intfa_transition(intfa_frame->intfa_state, ch);
        if(!t) {
            /* Parse error: we encountered a character for which we have no
             * transition. */
            if(s->bound_grammar->error_char_cb)
                s->bound_grammar->error_char_cb(s, ch);
            return GZL_STATUS_ERROR;
        }
    }

    /* We have finished processing transitions for the previous byte.
     * Move on to the next byte. */
    s->offset.byte++;

    /* Deal with newlines.  This is all very single-byte-encoding specific for
     * the moment. */
    bool is_newline_char = (ch == 0x0A || ch == 0x0D);  /* LF and CR */
    if(is_newline_char) {
        if(!s->last_char_was_newline) {
            s->offset.line++;
            s->offset.column = 1;
        }
    }
    else
        s->offset.column++;
    s->last_char_was_newline = is_newline_char;

    /* Do the transition. */
    intfa_frame->intfa_state = t->dest_state;

    /* If the current state is final and there are no outgoing transitions,
     * we *know* we don't have to wait any longer for the longest match.
     * Transition the RTN or GLA now, for more on-line behavior. */
    if(intfa_frame->intfa_state->final &&
       (intfa_frame->intfa_state->num_transitions == 0)) {
        status = process_terminal(s, intfa_frame->intfa_state->final,
                                  &frame->start_offset,
                                  s->offset.byte - frame->start_offset.byte);
        if(status != GZL_STATUS_OK)
            return status;
        push_intfa_frame_for_gla_or_rtn(s);
    }
    return GZL_STATUS_OK;
}

/*
 * The rest of this file is the publicly-exposed API, documented in the
 * header file.
 */

enum gzl_status gzl_parse(struct gzl_parse_state *s, char *buf, size_t buf_len)
{
    enum gzl_status status = GZL_STATUS_OK;
    size_t i;

    /* For the first call, we need to push the initial frame and
     * descend from the starting frame until we hit an IntFA frame. */
    if(s->offset.byte == 0 && s->parse_stack_len == 0) {
        push_rtn_frame(s, &s->bound_grammar->grammar->rtns[0], &s->offset);
        bool entered_gla;
        status = descend_to_gla(s, &entered_gla, &s->offset);
        if(status == GZL_STATUS_OK) push_intfa_frame_for_gla_or_rtn(s);
    }
    if(s->parse_stack_len == 0) {
        /* This gzl_parse_state has already hit hard EOF previously. */
        return GZL_STATUS_HARD_EOF;
    }

    for(i = 0; i < buf_len && status == GZL_STATUS_OK; i++)
        status = do_intfa_transition(s, buf[i]);
    return status;
}

bool gzl_finish_parse(struct gzl_parse_state *s)
{
    size_t i;
    /* First deal with an open IntFA frame if there is one.  The frame must
     * be in a start state (in which case we back it out), a final state
     * (in which case we recognize and process the terminal), or both (in
     * which case we back out iff. we are in a GLA state with an EOF transition
     * out).  */
    struct gzl_parse_stack_frame *frame = DYNARRAY_GET_TOP(s->parse_stack);
    if(frame->frame_type == GZL_FRAME_TYPE_INTFA) {
        struct gzl_intfa_frame *intfa_frame = &frame->f.intfa_frame;
        if(intfa_frame->intfa_state->final &&
           intfa_frame->intfa_state == &intfa_frame->intfa->states[0]) {
            /* TODO: handle this case. */
            assert(false);
        } else if(intfa_frame->intfa_state->final) {
            process_terminal(s, intfa_frame->intfa_state->final,
                             &frame->start_offset,
                             s->offset.byte - frame->start_offset.byte);
        } else if(intfa_frame->intfa_state == &intfa_frame->intfa->states[0]) {
            /* Pop the frame like it never happened. */
            pop_intfa_frame(s);
        } else {
            /* IntFA is in neither a start nor a final state. 
             * This cannot be EOF. */
            return false;
        }
    }

    /* Next deal with an open GLA frame if there is one.  The frame must be in
     * a start state or have an outgoing EOF transition, else we are not at
     * valid EOF. */
    frame = DYNARRAY_GET_TOP(s->parse_stack);
    if(frame->frame_type == GZL_FRAME_TYPE_GLA) {
        struct gzl_gla_frame *gla_frame = &frame->f.gla_frame;
        if(gla_frame->gla_state == &gla_frame->gla->states[0]) {
            /* GLA is in a start state -- fine, we can just pop it as
             * if it never happened. */
            pop_gla_frame(s);
        } else {
            /* For this to still be valid EOF, this GLA state must have an
             * outgoing EOF transition, and we must take it now. */
            struct gzl_gla_transition *t =
                find_gla_transition(gla_frame->gla_state, NULL);
            if(!t) return false;

            /* process_terminal() wants an IntFA frame to pop. */
            push_empty_frame(s, GZL_FRAME_TYPE_INTFA, &s->offset);
            process_terminal(s, NULL, &s->offset, 0);

            /* Pop any GLA states that the previous may have pushed. */
            while(s->parse_stack_len > 0 &&
                  DYNARRAY_GET_TOP(s->parse_stack)->frame_type !=
                  GZL_FRAME_TYPE_RTN)
                pop_frame(s);
        }
    }

    /* Now we should have only RTN frames open.  Starting from the top, check
     * that each frame's dest_state is a final state (or the actual current
     * state in the bottommost frame). */
    if(s->parse_stack_len > 0) { /* will be 0 if we already hit hard EOF. */
        for(i = 0; i < s->parse_stack_len - 1; i++) {
            frame = &s->parse_stack[i];
            assert(frame->frame_type == GZL_FRAME_TYPE_RTN);
            struct gzl_rtn_frame *rtn_frame = &frame->f.rtn_frame;
            assert(rtn_frame->rtn_transition);
            if(!rtn_frame->rtn_transition->dest_state->is_final) return false;
        }

        frame = DYNARRAY_GET_TOP(s->parse_stack);
        struct gzl_rtn_frame *rtn_frame = &frame->f.rtn_frame;
        if(!rtn_frame->rtn_state->is_final) return false;

        /* We are truly in a state where EOF is ok.  Pop remaining RTN frames to
         * call callbacks appropriately. */
        while(s->parse_stack_len > 0)
        {
            /* What should we do if the user cancels while the final RTN frames
             * are being popped?  It's kind of a weird thing to do.  Options
             * are to ignore it (we're finishing the parse anyway) or to stop.
             * For now we ignore. */
            pop_rtn_frame(s);
        }
    }

    return true;
}

struct gzl_parse_state *gzl_alloc_parse_state()
{
    struct gzl_parse_state *state = malloc(sizeof(*state));
    INIT_DYNARRAY(state->parse_stack, 0, 16);
    INIT_DYNARRAY(state->token_buffer, 0, 2);
    return state;
}

struct gzl_parse_state *gzl_dup_parse_state(struct gzl_parse_state *orig)
{
    struct gzl_parse_state *copy = malloc(sizeof(*copy));
    /* This erroneously copies pointers to dynarrays, but we'll fix in a sec. */
    *copy = *orig;
    size_t i;

    INIT_DYNARRAY(copy->parse_stack, 0, 16);
    RESIZE_DYNARRAY(copy->parse_stack, orig->parse_stack_len);
    for(i = 0; i < orig->parse_stack_len; i++)
        copy->parse_stack[i] = orig->parse_stack[i];

    INIT_DYNARRAY(copy->token_buffer, 0, 2);
    RESIZE_DYNARRAY(copy->token_buffer, orig->token_buffer_len);
    for(i = 0; i < orig->token_buffer_len; i++)
        copy->token_buffer[i] = orig->token_buffer[i];

    return copy;
}

void gzl_free_parse_state(struct gzl_parse_state *s)
{
    FREE_DYNARRAY(s->parse_stack);
    FREE_DYNARRAY(s->token_buffer);
    free(s);
}

void gzl_init_parse_state(struct gzl_parse_state *s,
                          struct gzl_bound_grammar *bg)
{
    s->offset.byte = 0;
    s->offset.line = 1;
    s->offset.column = 1;
    s->open_terminal_offset = s->offset;
    s->last_char_was_newline = false;
    s->bound_grammar = bg;
    RESIZE_DYNARRAY(s->parse_stack, 0);
    RESIZE_DYNARRAY(s->token_buffer, 0);

    /* Currently each stack frame takes 28 bytes on a 32-bit machine, so a
     * stack depth of 500 is a modest 14kb of RAM.  500 frames of recursion is
     * far deeper than we would expect any real text to be */
    s->max_stack_depth = 500;

    /* Currently each token of lookahead takes 20 bytes on a 32-bit machine, so
     * a lookahead depth of 500 is 10kb of RAM.  Input text would have to be
     * truly pathological to require this much lookahead. */
    s->max_lookahead = 500;
}

enum gzl_status gzl_parse_file(struct gzl_parse_state *state,
                               FILE *file, void *user_data,
                               size_t max_buffer_size)
{
    struct gzl_buffer *buffer = malloc(sizeof(*buffer));
    INIT_DYNARRAY(buffer->buf, 0, 4096);
    buffer->buf_offset = 0;
    buffer->bytes_parsed = 0;
    buffer->user_data = user_data;
    state->user_data = buffer;

    /* The minimum amount of the data in the buffer that we want to be new data
     * each time.  This number shrinks as the amount of data we're preserving
     * from open tokens grows.  If the number is below this number we increase
     * our buffer size. */
    const size_t min_new_data = 4000;

    enum gzl_status status;
    bool is_eof = false;
    do {
        /* Make sure we have space for at least min_new_data new data. */
        size_t new_buf_size = buffer->buf_size;
        while(buffer->buf_len + min_new_data > new_buf_size)
          buffer->buf_size *= 2;
        if(new_buf_size > max_buffer_size) {
            status = GZL_STATUS_RESOURCE_LIMIT_EXCEEDED;
            break;
        }
        if(new_buf_size != buffer->buf_size) {
            buffer->buf_size = new_buf_size;
            buffer->buf = realloc(buffer->buf, new_buf_size);
        }
        size_t bytes_to_read = buffer->buf_size - buffer->buf_len;

        /* Do the I/O and check for errors. */
        size_t bytes_read = fread(buffer->buf + buffer->buf_len, 1,
                                  bytes_to_read, file);
        if(bytes_read < bytes_to_read) {
            if(ferror(file)) {
                status = GZL_STATUS_IO_ERROR;
                break;
            } else if(feof(file)) {
                is_eof = true;
            }
        }

        /* Do the parse.  Start past whatever bytes we previously saved. */
        char *parse_start = buffer->buf + buffer->buf_len;
        buffer->buf_len += bytes_read;
        status = gzl_parse(state, parse_start, bytes_read);

        /* Preserve all data from tokens that haven't been returned yet:
         *
         *         buf                                                 size len
         *         |                                                     |   |
         *         v                                                     v   v
         * Buffer: -----------------------------------------------------------
         *         ^    ^                                   ^         ^
         *         |    |                                   |         |
         *  buf_offset  |                                   |  state->offset
         *              |                                   |
         *       previous value of                  current value of
         *   state->open_terminal_offset       state->open_terminal_offset
         *
         *         |----| <-- Data we were previously saving.
         *
         *                 Data we should now be saving --> |------------|
         */

        size_t bytes_to_discard = state->open_terminal_offset.byte -
                                  buffer->buf_offset;
        size_t bytes_to_save = buffer->buf_size - bytes_to_discard;
        char *buf_to_save_from = buffer->buf + bytes_to_discard;
        assert(bytes_to_discard <= (size_t) buffer->buf_len);  /* hasn't overflowed. */

        memmove(buffer->buf, buf_to_save_from, bytes_to_save);
        buffer->buf_offset += bytes_to_discard;
        buffer->buf_len = bytes_to_save;
    } while(status == GZL_STATUS_OK && !is_eof);

    if(status == GZL_STATUS_HARD_EOF || (status == GZL_STATUS_OK && is_eof)) {
        if(gzl_finish_parse(state)) {
            if(!feof(file) || buffer->buf_len > 0) {
                /* There was data left over -- we hit grammar EOF before
                 * file EOF. */
                status = GZL_STATUS_OK;
            } else
                status = GZL_STATUS_PREMATURE_EOF_ERROR;
        } else
            status = GZL_STATUS_PREMATURE_EOF_ERROR;
    }

    FREE_DYNARRAY(buffer->buf);
    free(buffer);
    return status;
}

/*
 * Local Variables:
 * c-file-style: "bsd"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:et:sts=4:sw=4
 */
