#!/usr/bin/env ruby

require File.dirname(__FILE__) + "/../lib/gazelle"

begin
  require 'jeweler'
  Jeweler::Tasks.new(Gazelle::Gemspec.spec)
rescue LoadError
  puts "Jeweler not available. Install it with: sudo gem install technicalpickles-jeweler -s http://gems.github.com"
end


require 'rake/extensiontask'

Rake::ExtensionTask.new('gazelle', Gazelle::Gemspec.spec)
