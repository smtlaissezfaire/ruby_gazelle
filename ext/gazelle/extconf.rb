require 'mkmf'

$CFLAGS += " -W -Wall"

dir_config("gazelle")
create_makefile("gazelle")
