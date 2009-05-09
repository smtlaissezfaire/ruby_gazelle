
module Gazelle
  class Parser
    def initialize(filename)
      file = expand_path(filename)
      raise(Errno::ENOENT) unless File.exists?(file)
      
      @filename = file
      @on = {}
    end
    
    def on(sym, &block)
      @on[sym] = block
    end
    
    def rules(&block)
      instance_eval(&block)
    end
    
  private
  
    def run_rule(name, str)
      @on[name.to_sym].call(str)
    end
  
    def expand_path(filename)
      File.expand_path(filename)
    end
  end
end

require File.dirname(__FILE__) + "/gazelle.bundle"
