# vim: set fileencoding=utf-8 :

# This code is original from jsmin by Douglas Crockford, it was translated to
# Python by Baruch Even. It was rewritten by Dave St.Germain for speed.
#
# The MIT License (MIT)
#·
# Copyright (c) 2013 Dave St.Germain
#·
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#·
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#·
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import unittest
import jsmin


class JsTests(unittest.TestCase):
    def _minify(self, js):
        return jsmin.jsmin(js)

    def assertEqual(self, thing1, thing2):
        if thing1 != thing2:
            print(repr(thing1), repr(thing2))
            raise AssertionError
        return True
    
    def assertMinified(self, js_input, expected, **kwargs):
        minified = jsmin.jsmin(js_input, **kwargs)
        assert minified == expected, "\ngot: %r\nexp: %r" % (minified, expected)
        
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
        expected = r"""if(Object.isFunction(Array.prototype.forEach))
Array.prototype._each=Array.prototype.forEach;if(!Array.prototype.indexOf)Array.prototype.indexOf=function(item,i){function(){foo;location='http://foo.com;';}"""
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

    def testBlockCommentStartingWithSlash(self):
        self.assertMinified('A; /*/ comment */ B', 'A;B')

    def testBlockCommentEndingWithSlash(self):
        self.assertMinified('A; /* comment /*/ B', 'A;B')

    def testLeadingBlockCommentStartingWithSlash(self):
        self.assertMinified('/*/ comment */ A', 'A')

    def testLeadingBlockCommentEndingWithSlash(self):
        self.assertMinified('/* comment /*/ A', 'A')

    def testEmptyBlockComment(self):
        self.assertMinified('/**/ A', 'A')

    def testBlockCommentMultipleOpen(self):
        self.assertMinified('/* A /* B */ C', 'C')

    def testJustAComment(self):
        self.assertMinified('     // a comment', '')

    def test_issue_bitbucket_10(self):
        js = '''
        files = [{name: value.replace(/^.*\\\\/, '')}];
        // comment
        A
        '''
        expected = '''files=[{name:value.replace(/^.*\\\\/,'')}];A'''
        self.assertMinified(js, expected)

    def test_issue_bitbucket_10_without_semicolon(self):
        js = '''
        files = [{name: value.replace(/^.*\\\\/, '')}]
        // comment
        A
        '''
        expected = '''files=[{name:value.replace(/^.*\\\\/,'')}]\nA'''
        self.assertMinified(js, expected)

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
    
    def testLiteralRe(self):
        js = r"""
        myString.replace(/\\/g, '/');
        console.log("hi");
        """
        expected = r"""myString.replace(/\\/g,'/');console.log("hi");"""
        self.assertMinified(js, expected)
        
        js = r''' return /^data:image\//i.test(url) || 
        /^(https?|ftp|file|about|chrome|resource):/.test(url);
        '''
        expected = r'''return /^data:image\//i.test(url)||/^(https?|ftp|file|about|chrome|resource):/.test(url);'''
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
        js_without_comment = r"""
        onSuccess: function(transport) {
            var js = transport.responseText.strip();
            if (!/^\[.*\]$/.test(js))
              throw 'Server returned an invalid collection representation.';
            this._collection = eval(js);
            this.checkForExternalText();
          }.bind(this),
          onFailure: this.onFailure
        });
        """
        self.assertMinified(js_without_comment, expected)

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
        self.assertMinified("{a: 1//comment\r\n}", "{a:1}")

    def testImplicitSemicolon(self):
        # return \n 1  is equivalent with   return; 1
        # so best make sure jsmin retains the newline
        self.assertMinified("return\na", "return\na")

    def test_explicit_semicolon(self):
        self.assertMinified("return;//comment\r\na", "return;a")

    def testImplicitSemicolon2(self):
        self.assertMinified("return//comment...\r\nar", "return\nar")

    def testImplicitSemicolon3(self):
        self.assertMinified("return//comment...\r\na", "return\na")

    def testSingleComment2(self):
        self.assertMinified('x.replace(/\//, "_")// slash to underscore',
                'x.replace(/\//,"_")')

    def testSlashesNearComments(self):
        original = '''
        { a: n / 2, }
        // comment
        '''
        expected = '''{a:n/2,}'''
        self.assertMinified(original, expected)
    
    def testReturn(self):
        original = '''
        return foo;//comment
        return bar;'''
        expected = 'return foo;return bar;'
        self.assertMinified(original, expected)
        original = '''
        return foo
        return bar;'''
        expected = 'return foo\nreturn bar;'
        self.assertMinified(original, expected)

    def test_space_plus(self):
        original = '"s" + ++e + "s"'
        expected = '"s"+ ++e+"s"'
        self.assertMinified(original, expected)

    def test_no_final_newline(self):
        original = '"s"'
        expected = '"s"'
        self.assertMinified(original, expected)

    def test_space_with_regex_repeats(self):
        original = '/(NaN| {2}|^$)/.test(a)&&(a="M 0 0");'
        self.assertMinified(original, original)  # there should be nothing jsmin can do here

    def test_space_with_regex_repeats_not_at_start(self):
        original = 'aaa;/(NaN| {2}|^$)/.test(a)&&(a="M 0 0");'
        self.assertMinified(original, original)  # there should be nothing jsmin can do here

    def test_space_in_regex(self):
        original = '/a (a)/.test("a")'
        self.assertMinified(original, original)

    def test_brackets_around_slashed_regex(self):
        original = 'function a() { /\//.test("a") }'
        expected = 'function a(){/\//.test("a")}'
        self.assertMinified(original, expected)

    def test_angular_1(self):
        original = '''var /** holds major version number for IE or NaN for real browsers */
                      msie,
                      jqLite,           // delay binding since jQuery could be loaded after us.'''
        minified = jsmin.jsmin(original)
        self.assertTrue('var\nmsie' in minified)

    def test_angular_2(self):
        original = 'var/* comment */msie;'
        expected = 'var msie;'
        self.assertMinified(original, expected)

    def test_angular_3(self):
        original = 'var /* comment */msie;'
        expected = 'var msie;'
        self.assertMinified(original, expected)

    def test_angular_4(self):
        original = 'var /* comment */ msie;'
        expected = 'var msie;'
        self.assertMinified(original, expected)

    def test_angular_5(self):
        original = 'a/b'
        self.assertMinified(original, original)

    def testBackticks(self):
        original = '`test`'
        self.assertMinified(original, original, quote_chars="'\"`")

        original = '` test with leading whitespace`'
        self.assertMinified(original, original, quote_chars="'\"`")

        original = '`test with trailing whitespace `'
        self.assertMinified(original, original, quote_chars="'\"`")

        original = '''`test
with a new line`'''
        self.assertMinified(original, original, quote_chars="'\"`")

        original = '''dumpAvStats: function(stats) {
        var statsString = "";
        if (stats.mozAvSyncDelay) {
          statsString += `A/V sync: ${stats.mozAvSyncDelay} ms `;
        }
        if (stats.mozJitterBufferDelay) {
          statsString += `Jitter-buffer delay: ${stats.mozJitterBufferDelay} ms`;
        }

        return React.DOM.div(null, statsString);'''
        expected = 'dumpAvStats:function(stats){var statsString="";if(stats.mozAvSyncDelay){statsString+=`A/V sync: ${stats.mozAvSyncDelay} ms `;}\nif(stats.mozJitterBufferDelay){statsString+=`Jitter-buffer delay: ${stats.mozJitterBufferDelay} ms`;}\nreturn React.DOM.div(null,statsString);'
        self.assertMinified(original, expected, quote_chars="'\"`")

    def testBackticksExpressions(self):
        original = '`Fifteen is ${a + b} and not ${2 * a + b}.`'
        self.assertMinified(original, original, quote_chars="'\"`")

        original = '''`Fifteen is ${a +
b} and not ${2 * a + "b"}.`'''
        self.assertMinified(original, original, quote_chars="'\"`")

    def testBackticksTagged(self):
        original = 'tag`Hello ${ a + b } world ${ a * b}`;'
        self.assertMinified(original, original, quote_chars="'\"`")

    def test_issue_bitbucket_16(self):
        original = """
            f = function() {
                return /DataTree\/(.*)\//.exec(this._url)[1];
            }
        """
        self.assertMinified(
            original,
            'f=function(){return /DataTree\/(.*)\//.exec(this._url)[1];}')

    def test_issue_bitbucket_17(self):
        original = "// hi\n/^(get|post|head|put)$/i.test('POST')"
        self.assertMinified(original,
                            "/^(get|post|head|put)$/i.test('POST')")

    def test_issue_6(self):
        original = '''
            respond.regex = {
                comments: /\/\*[^*]*\*+([^/][^*]*\*+)*\//gi,
                urls: 'whatever'
            };
        '''
        expected = original.replace(' ', '').replace('\n', '')
        self.assertMinified(original, expected)

    def test_issue_9(self):
        original = '\n'.join([
            'var a = \'hi\' // this is a comment',
            'var a = \'hi\' /* this is also a  comment */',
            'console.log(1) // this is a comment',
            'console.log(1) /* this is also a comment */',
            '1 // this is a comment',
            '1 /* this is also a comment */',
            '{} // this is a comment',
            '{} /* this is also a comment */',
            '"YOLO" /* this is a comment */',
            '"YOLO" // this is a comment',
            '(1 + 2) // comment',
            '(1 + 2) /* yup still comment */',
            'var b'
        ])
        expected = '\n'.join([
            'var a=\'hi\'',
            'var a=\'hi\'',
            'console.log(1)',
            'console.log(1)',
            '1',
            '1',
            '{}',
            '{}',
            '"YOLO"',
            '"YOLO"',
            '(1+2)',
            '(1+2)',
            'var b'
        ])
        self.assertMinified(expected, expected)
        self.assertMinified(original, expected)

    def test_newline_between_strings(self):
        self.assertMinified('"yolo"\n"loyo"', '"yolo"\n"loyo"')

    def test_issue_10_comments_between_tokens(self):
        self.assertMinified('var/* comment */a', 'var a')

    def test_ends_with_string(self):
        self.assertMinified('var s = "s"', 'var s="s"')

    def test_short_comment(self):
        self.assertMinified('a;/**/b', 'a;b')

    def test_shorter_comment(self):
        self.assertMinified('a;/*/*/b', 'a;b')

    def test_block_comment_with_semicolon(self):
        self.assertMinified('a;/**/\nb', 'a;b')

    def test_block_comment_With_implicit_semicolon(self):
        self.assertMinified('a/**/\nvar b', 'a\nvar b')

    def test_issue_9_single_comments(self):
        original = '''
            var a = "hello" // this is a comment
                       a += " world"
        '''
        self.assertMinified(original, 'var a="hello"\na+=" world"')

    def test_issue_9_multi_comments(self):
        original = '''
            var a = "hello" /* this is a comment */
                       a += " world"
        '''
        self.assertMinified(original, 'var a="hello"\na+=" world"')

    def test_issue_12_re_nl_if(self):
        original = '''
            var re = /\d{4}/
            if (1) { console.log(2); }'''
        self.assertMinified(
            original, 'var re=/\d{4}/\nif(1){console.log(2);}')

    def test_issue_12_re_nl_other(self):
        original = '''
            var re = /\d{4}/
            g = 10'''
        self.assertMinified(original , 'var re=/\d{4}/\ng=10')

    def test_preserve_copyright(self):
        original = '''
            function this() {
                /*! Copyright year person */
                console.log('hello!');
            }

            /*! Copyright blah blah
             *
             *  Some other text
             */

            var a;
        '''
        expected = """function this(){/*! Copyright year person */
console.log('hello!');}/*! Copyright blah blah
             *
             *  Some other text
             */\n\nvar a;"""
        self.assertMinified(original, expected)

    def test_issue_14(self):
        original = 'return x / 1;'
        self.assertMinified(original, 'return x/1;')
        
    def test_issue_14_with_char_from_return(self):
        original = 'return r / 1;'
        self.assertMinified(original, 'return r/1;')


class RegexTests(unittest.TestCase):

    def regex_recognise(self, js):
        if not jsmin.is_3:
            if jsmin.cStringIO and not isinstance(js, unicode):
                # strings can use cStringIO for a 3x performance
                # improvement, but unicode (in python2) cannot
                klass = jsmin.cStringIO.StringIO
            else:
                klass = jsmin.StringIO.StringIO
        else:
            klass = jsmin.io.StringIO
        ins = klass(js[2:])
        outs = klass()
        jsmin.JavascriptMinify(ins, outs).regex_literal(js[0], js[1])
        return outs.getvalue()

    def assert_regex(self, js_input, expected):
        assert js_input[0] == '/'  # otherwise we should not be testing!
        recognised = self.regex_recognise(js_input)
        assert recognised == expected, "\n in: %r\ngot: %r\nexp: %r" % (js_input, recognised, expected)

    def test_simple(self):
        self.assert_regex('/123/g', '/123/')

    def test_character_class(self):
        self.assert_regex('/a[0-9]b/g', '/a[0-9]b/')

    def test_character_class_with_slash(self):
        self.assert_regex('/a[/]b/g', '/a[/]b/')

    def test_escaped_forward_slash(self):
        self.assert_regex(r'/a\/b/g', r'/a\/b/')

    def test_escaped_back_slash(self):
        self.assert_regex(r'/a\\/g', r'/a\\/')

    def test_empty_character_class(self):
        # This one is subtle: an empty character class is not allowed, afaics
        # from http://regexpal.com/ Chrome Version 44.0.2403.155 (64-bit) Mac
        # so this char class is interpreted as containing ]/ *not* as char
        # class [] followed by end-of-regex /.
        self.assert_regex('/a[]/]b/g', '/a[]/]b/')

    def test_precedence_of_parens(self):
        # judging from
        # http://regexpal.com/ Chrome Version 44.0.2403.155 (64-bit) Mac
        # () have lower precedence than []
        self.assert_regex('/a([)])b/g', '/a([)])b/')
        self.assert_regex('/a[(]b/g', '/a[(]b/')

if __name__ == '__main__':
    unittest.main()
