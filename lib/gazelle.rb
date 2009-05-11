
require "rubygems"
require "using"

Using.default_load_scheme = :autoload

module Gazelle
  extend Using
  using :Parser
  using :Gemspec
end

require File.dirname(__FILE__) + "/gazelle.bundle"
