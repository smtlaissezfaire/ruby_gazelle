#ifndef GAZELLE_RUBY_PARSE
#define GAZELLE_RUBY_PARSE

#include <ruby.h>
#include <gazelle/dynarray.h>
#include "bc_read_stream.c"
#include "load_grammar.c"
#include "parse.c"

#define RSTRING_TO_PTR(x) RSTRING(x)->ptr

static int terminal_error = 0;

static void error_char_callback() {
  // do something intelligent here
}

static void error_terminal_callback() {
  terminal_error = 1;
}

static void reset_terminal_error() {
  terminal_error = 0;
}

static void rb_gzl_parse(char *input, struct gzl_parse_state *state, struct gzl_bound_grammar *bg) {
  gzl_init_parse_state(state, bg);
  gzl_parse(state, input, strlen(input) + 1);
  gzl_finish_parse(state);
}

static int run_grammar(char *filename, char *input) {
  reset_terminal_error();
  
  struct bc_read_stream *s = bc_rs_open_file(filename);
  if (!s) {
    return 1; // should raise an invalid file format error in ruby instead
  }
  struct gzl_grammar *g = gzl_load_grammar(s);
  bc_rs_close_stream(s);
  
  struct gzl_parse_state *state = gzl_alloc_parse_state();
  struct gzl_bound_grammar bg = {
    .grammar           = g,
    .error_char_cb     = error_char_callback,
    .error_terminal_cb = error_terminal_callback
  };
  
  rb_gzl_parse(input, state, &bg);

  return 0;
}

static VALUE rb_gazelle_parse_p(VALUE self, VALUE input) {
  VALUE compiled_file_stream = rb_iv_get(self, "@filename");
  char *filename     = RSTRING_TO_PTR(compiled_file_stream);
  char *input_string = RSTRING_TO_PTR(input);
  
  if (run_grammar(filename, input_string)) {
    return Qfalse;
  }

  return(terminal_error ? Qfalse : Qtrue);
}

void Init_gazelle() {
  VALUE Gazelle         = rb_const_get(rb_cObject, rb_intern("Gazelle"));
  VALUE Gazelle_Parser  = rb_const_get_at(Gazelle, rb_intern("Parser"));

  rb_define_method(Gazelle_Parser, "parse?", rb_gazelle_parse_p, 1);
}

#endif /* GAZELLE_RUBY_PARSE */
