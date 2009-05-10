#!/usr/bin/env ruby

PROJECT_NAME = "gazelle"

begin
  require 'jeweler'
  spec = Gem::Specification.new do |s|
    s.name        = "#{PROJECT_NAME}"
    s.summary     = "Ruby bindings for the Gazelle parser-generator"
    s.email       = "scott@railsnewbie.com"
    s.homepage    = "http://github.com/smtlaissezfaire/#{PROJECT_NAME.downcase}"
    s.description = "TODO"
    s.authors     = ["Scott Taylor"]

    s.platform    = Gem::Platform::RUBY
    s.extensions  = FileList["ext/**/extconf.rb"]
    s.files       = FileList[
                      "ext/**/*",
                      "lib/**/*.rb",
                      "spec/**/**",
                      "Rakefile",
                      "tasks/**/*.rake"
                    ]
  end

  Jeweler::Tasks.new(spec)
rescue LoadError
  puts "Jeweler not available. Install it with: sudo gem install technicalpickles-jeweler -s http://gems.github.com"
end


require 'rake/extensiontask'

Rake::ExtensionTask.new('gazelle', spec)
