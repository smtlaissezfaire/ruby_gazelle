module Gazelle
  class Parser
    include DebuggingSupport
    
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

    attr_writer :debug

    def run_rule(action, str)
      with_action(action, str) do |rule|
        rule.call(str)
      end
    end

private

    def with_action(action, str)
      debugging = debugging?
      stream    = self.debug_stream

      stream << "rule: '#{action}'\n  string: '#{str}'\n" if debugging

      if rule = @rules[action.to_sym]
        stream << "  YIELDING TO RULE\n" if debugging
        yield(rule)
      end
    end
  
    def expand_path(filename)
      File.expand_path(filename)
    end
  end
end

require File.dirname(__FILE__) + "/../gazelle_ruby_bindings"
