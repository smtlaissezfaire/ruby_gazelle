require File.dirname(__FILE__) + "/../lib/gazelle"

require 'rake/extensiontask'
Rake::ExtensionTask.new('gazelle_ruby_bindings', Gazelle::Gemspec.spec)
