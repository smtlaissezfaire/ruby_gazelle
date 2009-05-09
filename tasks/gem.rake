#!/usr/bin/env ruby

PROJECT_NAME = "ruby_gazelle"

begin
  require 'jeweler'
  Jeweler::Tasks.new do |s|
    s.name        = "#{PROJECT_NAME}"
    s.summary     = "Ruby bindings for the Gazelle parser-generator"
    s.email       = "scott@railsnewbie.com"
    s.homepage    = "http://github.com/smtlaissezfaire/#{PROJECT_NAME.downcase}"
    s.description = "TODO"
    s.authors     = ["Scott Taylor"]
  end
rescue LoadError
  puts "Jeweler not available. Install it with: sudo gem install technicalpickles-jeweler -s http://gems.github.com"
end
