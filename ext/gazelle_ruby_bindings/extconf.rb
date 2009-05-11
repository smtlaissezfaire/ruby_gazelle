require 'mkmf'

$CFLAGS += " -W -Wall"

dir_config("gazelle_ruby_bindings")
create_makefile("gazelle_ruby_bindings")
