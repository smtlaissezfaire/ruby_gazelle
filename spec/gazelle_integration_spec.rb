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
  end
end
