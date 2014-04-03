import unittest
import jsmin
import sys

class JsTests(unittest.TestCase):
    def _minify(self, js):
        return jsmin.jsmin(js)

    def assertEqual(self, thing1, thing2):
        if thing1 != thing2:
            print(repr(thing1), repr(thing2))
            raise AssertionError
        return True
    
    def assertMinified(self, js_input, expected):
        minified = jsmin.jsmin(js_input)
        assert minified == expected, "%r != %r" % (minified, expected)
        
    def testQuoted(self):
        js = r'''
        Object.extend(String, {
          interpret: function(value) {
            return value == null ? '' : String(value);
          },
          specialChar: {
            '\b': '\\b',
            '\t': '\\t',
            '\n': '\\n',
            '\f': '\\f',
            '\r': '\\r',
            '\\': '\\\\'
          }
        });

        '''
        expected = r"""Object.extend(String,{interpret:function(value){return value==null?'':String(value);},specialChar:{'\b':'\\b','\t':'\\t','\n':'\\n','\f':'\\f','\r':'\\r','\\':'\\\\'}});"""
        self.assertMinified(js, expected)

    def testSingleComment(self):
        js = r'''// use native browser JS 1.6 implementation if available
        if (Object.isFunction(Array.prototype.forEach))
          Array.prototype._each = Array.prototype.forEach;

        if (!Array.prototype.indexOf) Array.prototype.indexOf = function(item, i) {

        // hey there
        function() {// testing comment
        foo;
        //something something

        location = 'http://foo.com;';   // goodbye
        }
        //bye
        '''
        expected = r""" 
if(Object.isFunction(Array.prototype.forEach))
Array.prototype._each=Array.prototype.forEach;if(!Array.prototype.indexOf)Array.prototype.indexOf=function(item,i){ function(){ foo; location='http://foo.com;';}"""
        # print expected
        self.assertMinified(js, expected)
    
    def testEmpty(self):
        self.assertMinified('', '')
        self.assertMinified(' ', '')
        self.assertMinified('\n', '')
        self.assertMinified('\r\n', '')
        self.assertMinified('\t', '')
        
        
    def testMultiComment(self):
        js = r"""
        function foo() {
            print('hey');
        }
        /*
        if(this.options.zindex) {
          this.originalZ = parseInt(Element.getStyle(this.element,'z-index') || 0);
          this.element.style.zIndex = this.options.zindex;
        }
        */
        another thing;
        """
        expected = r"""function foo(){print('hey');}
another thing;"""
        self.assertMinified(js, expected)
    
    def testLeadingComment(self):
        js = r"""/* here is a comment at the top
        
        it ends here */
        function foo() {
            alert('crud');
        }
        
        """
        expected = r"""function foo(){alert('crud');}"""
        self.assertMinified(js, expected)
        
    def testJustAComment(self):
        self.assertMinified('     // a comment', '')
        
    def testRe(self):
        js = r'''  
        var str = this.replace(/\\./g, '@').replace(/"[^"\\\n\r]*"/g, '');
        return (/^[,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]*$/).test(str);
        });'''
        expected = r"""var str=this.replace(/\\./g,'@').replace(/"[^"\\\n\r]*"/g,'');return(/^[,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]*$/).test(str);});"""
        self.assertMinified(js, expected)

    def testIgnoreComment(self):
        js = r"""
        var options_for_droppable = {
          overlap:     options.overlap,
          containment: options.containment,
          tree:        options.tree,
          hoverclass:  options.hoverclass,
          onHover:     Sortable.onHover
        }

        var options_for_tree = {
          onHover:      Sortable.onEmptyHover,
          overlap:      options.overlap,
          containment:  options.containment,
          hoverclass:   options.hoverclass
        }

        // fix for gecko engine
        Element.cleanWhitespace(element); 
        """
        expected = r"""var options_for_droppable={overlap:options.overlap,containment:options.containment,tree:options.tree,hoverclass:options.hoverclass,onHover:Sortable.onHover}
var options_for_tree={onHover:Sortable.onEmptyHover,overlap:options.overlap,containment:options.containment,hoverclass:options.hoverclass} 
Element.cleanWhitespace(element);"""
        self.assertMinified(js, expected)

    def testHairyRe(self):
        js = r"""
        inspect: function(useDoubleQuotes) {
          var escapedString = this.gsub(/[\x00-\x1f\\]/, function(match) {
            var character = String.specialChar[match[0]];
            return character ? character : '\\u00' + match[0].charCodeAt().toPaddedString(2, 16);
          });
          if (useDoubleQuotes) return '"' + escapedString.replace(/"/g, '\\"') + '"';
          return "'" + escapedString.replace(/'/g, '\\\'') + "'";
        },

        toJSON: function() {
          return this.inspect(true);
        },

        unfilterJSON: function(filter) {
          return this.sub(filter || Prototype.JSONFilter, '#{1}');
        },
        """
        expected = r"""inspect:function(useDoubleQuotes){var escapedString=this.gsub(/[\x00-\x1f\\]/,function(match){var character=String.specialChar[match[0]];return character?character:'\\u00'+match[0].charCodeAt().toPaddedString(2,16);});if(useDoubleQuotes)return'"'+escapedString.replace(/"/g,'\\"')+'"';return"'"+escapedString.replace(/'/g,'\\\'')+"'";},toJSON:function(){return this.inspect(true);},unfilterJSON:function(filter){return this.sub(filter||Prototype.JSONFilter,'#{1}');},"""
        self.assertMinified(js, expected)

    def testNoBracesWithComment(self):
        js = r"""
        onSuccess: function(transport) {
            var js = transport.responseText.strip();
            if (!/^\[.*\]$/.test(js)) // TODO: improve sanity check
              throw 'Server returned an invalid collection representation.';
            this._collection = eval(js);
            this.checkForExternalText();
          }.bind(this),
          onFailure: this.onFailure
        });
        """
        expected = r"""onSuccess:function(transport){var js=transport.responseText.strip();if(!/^\[.*\]$/.test(js)) 
throw'Server returned an invalid collection representation.';this._collection=eval(js);this.checkForExternalText();}.bind(this),onFailure:this.onFailure});"""
        self.assertMinified(js, expected)
    
    def testSpaceInRe(self):
        js = r"""
        num = num.replace(/ /g,'');
        """
        self.assertMinified(js, "num=num.replace(/ /g,'');")
    
    def testEmptyString(self):
        js = r'''
        function foo('') {
        
        }
        '''
        self.assertMinified(js, "function foo(''){}")
    
    def testDoubleSpace(self):
        js = r'''
var  foo    =  "hey";
        '''
        self.assertMinified(js, 'var foo="hey";')
    
    def testLeadingRegex(self):
        js = r'/[d]+/g    '
        self.assertMinified(js, js.strip())
    
    def testLeadingString(self):
        js = r"'a string in the middle of nowhere'; // and a comment"
        self.assertMinified(js, "'a string in the middle of nowhere';")
    
    def testSingleCommentEnd(self):
        js = r'// a comment\n'
        self.assertMinified(js, '')
    
    def testInputStream(self):
        try:
            from StringIO import StringIO
        except ImportError:
            from io import StringIO
            
        ins = StringIO(r'''
            function foo('') {

            }
            ''')
        outs = StringIO()
        m = jsmin.JavascriptMinify()
        m.minify(ins, outs)
        output = outs.getvalue()
        assert output == "function foo(''){}"
    
    def testUnicode(self):
        instr = u'\u4000 //foo'
        expected = u'\u4000'
        output = jsmin.jsmin(instr)
        self.assertEqual(output, expected)

    def testCommentBeforeEOF(self):
        self.assertMinified("//test\r\n", "")
    
    def testCommentInObj(self):
        self.assertMinified("""{ 
            a: 1,//comment
            }""", "{a:1,}")

    def testCommentInObj2(self):
        self.assertMinified("{a: 1//comment\r\n}", "{a:1\n}")

    def testImplicitSemicolon(self):
        # return \n 1  is equivalent with   return; 1
        # so best make sure jsmin retains the newline
        self.assertMinified("return;//comment\r\na", "return;a")

    def testImplicitSemicolon2(self):
        self.assertMinified("return//comment...\r\na", "return\na")
    
    def testSingleComment2(self):
        self.assertMinified('x.replace(/\//, "_")// slash to underscore',
                'x.replace(/\//,"_")')


if __name__ == '__main__':
    unittest.main()
