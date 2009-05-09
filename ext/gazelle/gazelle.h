#ifndef GAZELLE_RUBY_PARSE_H
#define GAZELLE_RUBY_PARSE_H

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

void Init_gazelle();

#endif /* GAZELLE_RUBY_PARSE_H */