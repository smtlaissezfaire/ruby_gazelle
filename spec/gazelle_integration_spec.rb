require File.dirname(__FILE__) + "/spec_helper"

module Gazelle
  describe Parser do
    describe "parse" do
      it "should be true if it can parse the input" do
        parser = Parser.new(File.dirname(__FILE__) + "/hello.gzc")
        parser.parse?("(5)").should be_true
      end

      it "should be false if it cannot parse the input" do
        parser = Parser.new(File.dirname(__FILE__) + "/hello.gzc")
        parser.parse?("(()").should be_false
      end

      it "should find the file even when given with a short path" do
        parser = Parser.new("spec/hello.gzc")
        parser.parse("(5)").should be_true
      end

      it "should find the file when it is missing the .gzc extension" do
        parser = Parser.new("spec/hello")
        parser.parse("(5)").should be_true
      end

      it "should raise an 'Errno::ENOENT' error if the file does not exist" do
        file = "#{File.dirname(__FILE__)}/non-existant-file"
        
        lambda {
          parser = Parser.new(file)
        }.should raise_error(Errno::ENOENT, "No such file or directory")
      end

      it "should not parse when the file is not in a valid format" do
        file = "#{File.dirname(__FILE__)}/invalid_format.gzc"
        parser = Parser.new(file)

        parser.parse?("foo").should be_false
      end
    end
    
    describe "running an action" do
      before do
        @parser = Parser.new(File.dirname(__FILE__) + "/hello.gzc")
      end
      
      it "should not run the action if it isn't triggered" do
        run = false
        
        @parser.on :hello do
          run = true
        end
        
        @parser.parse("(()")
        
        run.should be_false
      end
      
      it "should run the action when it is triggered" do
        run = false
        
        @parser.on :hello do
          run = true
        end
        
        @parser.parse("(5)")
        
        run.should be_true
      end
      
      it "should yield the text" do
        yielded_text = nil
        
        @parser.on :hello do |text|
          yielded_text = text
        end
        
        @parser.parse("(5)")
        yielded_text.should == "(5)"
      end
      
      it "should yield the correct text" do
        yielded_text = nil
        
        @parser.on :hello do |text|
          yielded_text = text
        end
        
        @parser.parse("((1923423))")
        yielded_text.should == "((1923423))"
      end
      
      it "should be able to parse a rule with a block" do
        yielded_text = nil
        
        @parser.rules do
          on :hello do |text|
            yielded_text = text
          end
        end
        
        @parser.parse("(5)")
        yielded_text.should == "(5)"
      end
      
      it "should not raise an error if there is no action for a given rule" do
        lambda {
          @parser.parse("(5)")
        }.should_not raise_error
      end
      
      it "should return true when the parse matches, but there is no triggered rule" do
        @parser.parse("(5)").should be_true
      end
      
      it "should be able to call a rule defined with a string, but called with a symbol" do
        called = false
        
        @parser.on "hello" do
          called = true
        end
        
        @parser.parse("(5)")
        called.should be_true
      end
    end
    
    describe "parsing subnodes" do
      context "a create statement in a sql parser" do
        before do
          @parser = Gazelle::Parser.new(File.dirname(__FILE__) + "/create_table.gzc")
        end

        it "should yield the correct table name + column name" do
          yielded_text = []

          @parser.on :UNQUOTED_ID do |text|
            yielded_text << text
          end

          @parser.parse("CREATE TABLE foo (bar BIT)")
          yielded_text.should == ["foo", "bar"]
        end

        it "should yield the column_names_and_types subnode" do
          pending 'TODO: ISSUE #5 on github' do
            yielded_text = nil
            
            @parser.on :column_names_and_types do |str|
              yielded_text = str
            end
   
            @parser.parse("CREATE TABLE foo (bar BIT)")
            yielded_text.should == "bar BIT"
          end
        end

        it "should yield the correct ID (without spaces or extra crap)" do
          pending "FIXME" do
            yielded_text = []
   
            @parser.on :ID do |text|
              yielded_text << text
            end
   
            @parser.parse("CREATE TABLE foo (bar BIT)")
            yielded_text.should == ["foo", "bar"]
          end
        end
      end
    end

    describe "debugging" do
      before do
        @parser = Gazelle::Parser.new(File.dirname(__FILE__) + "/create_table.gzc")
        @debug_stream = ""
      end

      it "should be able to turn it on" do
        @parser.debug = true
        @parser.should be_debugging
      end

      it "should not be debugging by default" do
        @parser.should_not be_debugging
      end

      it "should return false to debugging?" do
        @parser.debugging?.should be_false
      end

      it "should have the default stream as stdout" do
        @parser.debug_stream.should equal($stdout)
      end

      it "should be able to set (and get) the debug stream" do
        @parser.debug_stream = @debug_stream
        @parser.debug_stream.should equal(@debug_stream)
      end

      describe "running a rule" do
        before do
          @parser
          @parser.debug = true
          @parser.debug_stream = @debug_stream
        end

        def run_parser
          @parser.parse("CREATE TABLE foo (bar BIT)")
        end

        it "should send the '<<' message to the stream with a rule, when run" do
          @debug_stream.should_receive(:<<).with(any_args).any_number_of_times.and_return true
          run_parser
        end

        it "should not have an empty stream if a rule is hit" do
          run_parser
          @debug_stream.should_not be_empty
        end

        it "should not send the message if not in debug mode" do
          @parser.debug = false
          run_parser
          @debug_stream.should be_empty
        end

        it "should give the rule name in the stream" do
          @parser.run_rule :foo, "something"
          @debug_stream.should include("rule: 'foo'\n  string: 'something'\n")
        end

        it "should use the correct rule name" do
          @parser.run_rule :bar, "something"
          @debug_stream.should include("rule: 'bar'\n  string: 'something'\n")
        end

        it "should use the string matched" do
          @parser.run_rule :foo, "bar"
          @debug_stream.should include("rule: 'foo'\n  string: 'bar'\n")
        end

        it "should add the knowledge of yielding to the rule" do
          @parser.on(:foo) { }
          
          @parser.run_rule :foo, "bar"
          @debug_stream.should include("rule: 'foo'\n  string: 'bar'\n  YIELDING TO RULE\n")
        end
      end
    end
  end
end
