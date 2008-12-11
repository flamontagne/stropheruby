require 'mkmf'
dir_config("expat")
have_library("expat")
dir_config("strophe")
have_library("strophe")
have_library("resolv")
create_makefile("strophe_ruby")
