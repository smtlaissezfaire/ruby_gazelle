# -*- encoding: utf-8 -*-

Gem::Specification.new do |s|
  s.name = %q{gazelle}
  s.version = "0.1.0"

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.authors = ["Scott Taylor"]
  s.date = %q{2009-05-10}
  s.description = %q{TODO}
  s.email = %q{scott@railsnewbie.com}
  s.extensions = ["ext/gazelle/extconf.rb"]
  s.files = ["ext/gazelle", "ext/gazelle/extconf.rb", "ext/gazelle/gazelle.c", "ext/gazelle/gazelle.h", "ext/gazelle/includes", "ext/gazelle/includes/bc_read_stream.c", "ext/gazelle/includes/load_grammar.c", "ext/gazelle/includes/parse.c", "lib/gazelle.rb", "spec/gazelle_integration_spec.rb", "spec/hello.gzc", "spec/hello.gzl", "spec/invalid_format.gzc", "spec/spec.opts", "spec/spec_helper.rb", "Rakefile", "tasks/flog.rake", "tasks/gem.rake", "tasks/rspec.rake", "tasks/sloc.rake", "tasks/tags.rake"]
  s.has_rdoc = true
  s.homepage = %q{http://github.com/smtlaissezfaire/gazelle}
  s.rdoc_options = ["--inline-source", "--charset=UTF-8"]
  s.require_paths = ["lib"]
  s.rubygems_version = %q{1.3.1}
  s.summary = %q{Ruby bindings for the Gazelle parser-generator}

  if s.respond_to? :specification_version then
    current_version = Gem::Specification::CURRENT_SPECIFICATION_VERSION
    s.specification_version = 2

    if Gem::Version.new(Gem::RubyGemsVersion) >= Gem::Version.new('1.2.0') then
    else
    end
  else
  end
end
