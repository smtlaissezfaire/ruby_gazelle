#ifndef GAZELLE_RUBY_BINDINGS_H
#define GAZELLE_RUBY_BINDINGS_H

#define RSTRING_TO_PTR(x) RSTRING(x)->ptr

struct rb_gzl_user_data {
  /* The pointer to the current ruby parser object. */
  VALUE self;
  
  /* The input given to the parse function */
  char *input;

  /* The real ruby string object given to the parser */
  VALUE rb_input;
};

typedef struct gzl_parse_state       ParseState;
typedef struct gzl_bound_grammar     BoundGrammar;
typedef struct rb_gzl_user_data      RbUserData;
typedef struct gzl_parse_stack_frame ParseStackFrame;
typedef struct rb_gzl_user_data      RbGazelleUserData;

void Init_gazelle_ruby_bindings();

#endif /* GAZELLE_RUBY_BINDINGS_H */
