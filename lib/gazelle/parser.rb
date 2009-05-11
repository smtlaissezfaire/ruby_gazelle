module Gazelle
  class Parser
    def initialize(filename)
      file = expand_path(filename)
      raise(Errno::ENOENT) unless File.exists?(file)
      
      @filename = file
      @rules = {}
    end
    
    def on(action, &block)
      @rules[action.to_sym] = block
    end
    
    def rules(&block)
      instance_eval(&block)
    end
    
  private
  
    def run_rule(action, str)
      if rule = @rules[action.to_sym]
        rule.call(str)
      end
    end
  
    def expand_path(filename)
      File.expand_path(filename)
    end
  end
end

require File.dirname(__FILE__) + "/../gazelle_ruby_bindings"
