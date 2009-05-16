
def gzlc(file)
  gzl_path = `which gzlc`.strip
  sh "#{gzl_path} #{file}"
end

task :compile_grammars do
  FileList["**/*.gzl"].each do |file|
    gzlc file
  end
end
