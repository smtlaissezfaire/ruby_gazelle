#ifndef GAZELLE_RUBY_PARSE
#define GAZELLE_RUBY_PARSE

#include <stdbool.h>
#include <ruby.h>
#include <gazelle/dynarray.h>
#include "bc_read_stream.c"
#include "load_grammar.c"
#include "parse.c"

#define RSTRING_TO_PTR(x) RSTRING(x)->ptr

typedef struct gzl_parse_state   ParseState;
typedef struct gzl_bound_grammar BoundGrammar;
typedef struct rb_gzl_user_data  RbUserData;

struct rb_gzl_user_data {
  /* The pointer to the current ruby parser object. */
  VALUE self;
  
  /* The input given to the parse function */
  char *input;
};

/* ERROR FUNCTIONS */
static int terminal_error = 0;

static void error_char_callback() {
  // TODO: do something intelligent here
}

static void error_terminal_callback() {
  terminal_error = 1;
}

static void reset_terminal_error() {
  terminal_error = 0;
}


/* General Gazelle integration */
static void rb_gzl_parse(char *input, ParseState *state, BoundGrammar *bg) {
  gzl_init_parse_state(state, bg);
  gzl_parse(state, input, strlen(input) + 1);
}

static VALUE user_data_obj(RbUserData *user_data) {
  return(user_data->self);
}

static char *user_data_input(RbUserData *user_data) {
  return(user_data->input);
}

static void end_rule_callback(ParseState *parse_state)
{
  struct gzl_parse_stack_frame *frame      = DYNARRAY_GET_TOP(parse_state->parse_stack);
  struct gzl_rtn_frame          *rtn_frame = &frame->f.rtn_frame;
  
  VALUE self            = user_data_obj(parse_state->user_data);
  char *rule_name       = rtn_frame->rtn->name;
  VALUE ruby_rule_name  = rb_str_new2(rule_name);
  char *input           = user_data_input(parse_state->user_data);
  VALUE ruby_input      = rb_str_new2(input);
  
  rb_funcall(self, rb_intern("run_rule"), 2, ruby_rule_name, ruby_input);
}

static void mk_user_data(ParseState *state, VALUE self, char *input) {
  RbUserData *data = malloc(sizeof(RbUserData *));
  data->self  = self;
  data->input = input;
  state->user_data = data;
}

static int run_grammar(VALUE self, char *filename, char *input, bool run_callbacks) {
  reset_terminal_error();
  
  struct bc_read_stream *s = bc_rs_open_file(filename);
  if (!s)
    return 1; // should raise an invalid file format error in ruby instead

  struct gzl_grammar *g = gzl_load_grammar(s);
  bc_rs_close_stream(s);
  
  ParseState *state = gzl_alloc_parse_state();
  mk_user_data(state, self, input);
  
  BoundGrammar bg = {
    .grammar           = g,
    .error_char_cb     = error_char_callback,
    .error_terminal_cb = error_terminal_callback
  };
  
  if (run_callbacks) {
    bg.end_rule_cb = end_rule_callback;
  }
  
  rb_gzl_parse(input, state, &bg);

  return 0;
}

static VALUE run_gazelle_parse(VALUE self, VALUE input, bool run_callbacks) {
  VALUE compiled_file_stream = rb_iv_get(self, "@filename");
  char *filename     = RSTRING_TO_PTR(compiled_file_stream);
  char *input_string = RSTRING_TO_PTR(input);
  
  if (run_grammar(self, filename, input_string, run_callbacks))
    return Qfalse;

  return(terminal_error ? Qfalse : Qtrue);
}

/* Public Ruby methods */

static VALUE rb_gazelle_parse_p(VALUE self, VALUE input) {
  return run_gazelle_parse(self, input, false);
}

static VALUE rb_gazelle_parse(VALUE self, VALUE input) {
  return run_gazelle_parse(self, input, true);
}

/* Hook up the ruby methods.  Similar to lua's luaopen_(mod) functions */

void Init_gazelle() {
  VALUE Gazelle         = rb_const_get(rb_cObject, rb_intern("Gazelle"));
  VALUE Gazelle_Parser  = rb_const_get_at(Gazelle, rb_intern("Parser"));

  rb_define_method(Gazelle_Parser, "parse?", rb_gazelle_parse_p, 1);
  rb_define_method(Gazelle_Parser, "parse",  rb_gazelle_parse, 1);
}

#endif /* GAZELLE_RUBY_PARSE */