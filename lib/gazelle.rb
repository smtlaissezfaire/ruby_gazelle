
require "rubygems"
require "using"

Using.default_load_scheme = :autoload

module Gazelle
  extend Using
  using :DebuggingSupport
  using :Parser
  using :Gemspec
end
