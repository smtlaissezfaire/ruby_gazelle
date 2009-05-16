module Gazelle
  module DebuggingSupport
    def debugging?
      @debug ? true : false
    end

    def debug_stream
      @debug_stream || $stdout
    end

    attr_writer :debug_stream
  end
end
