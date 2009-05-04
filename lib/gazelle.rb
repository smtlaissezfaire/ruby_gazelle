
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
    
    def run_rule(name)
      @on[name.to_sym].call
    end
    
  private

    def expand_path(filename)
      File.expand_path(filename)
    end
  end
end

require File.dirname(__FILE__) + "/../ext/gazelle/gazelle"
