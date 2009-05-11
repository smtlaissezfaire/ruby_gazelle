module Gazelle
  class Gemspec
    def self.spec(&block)
      new(&block).spec
    end

    PROJECT_NAME = "gazelle"

    def initialize(*args, &block)
      @spec = Gem::Specification.new do |s|
        summary = "Ruby bindings for the Gazelle parser-generator"
        
        s.name        = PROJECT_NAME
        s.summary     = summary
        s.description = summary
        
        s.email       = "scott@railsnewbie.com"
        s.homepage    = "http://github.com/smtlaissezfaire/#{PROJECT_NAME.downcase}"
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
        
        yield(s) if block_given?
      end
    end

    attr_reader :spec
  end
end
