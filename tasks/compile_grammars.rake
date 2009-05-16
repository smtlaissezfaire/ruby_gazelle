
module Gazelle
  module Compilation
    module_function

    def compile(file)
      gzl_path = `which gzlc`.strip
      sh "#{gzl_path} #{file}"
    end
  end
end

namespace :compile do
  task :grammars do
    FileList["**/*.gzl"].each do |file|
      Gazelle::Compilation.compile(file)
    end
  end
end

