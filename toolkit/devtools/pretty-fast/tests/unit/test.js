/*
 * Copyright 2013 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.md or:
 * http://opensource.org/licenses/BSD-2-Clause
 */
var prettyFast = this.prettyFast || require("./pretty-fast");

var testCases = [

  {
    name: "Simple function",
    input: "function foo() { bar(); }",
    output: "function foo() {\n" +
            "  bar();\n" +
            "}\n",
    mappings: [
      // function foo() {
      {
        inputLine: 1,
        outputLine: 1
      },
      // bar();
      {
        inputLine: 1,
        outputLine: 2
      },
      // }
      {
        inputLine: 1,
        outputLine: 3
      },
    ]
  },

  {
    name: "Nested function",
    input: "function foo() { function bar() { debugger; } bar(); }",
    output: "function foo() {\n" +
            "  function bar() {\n" +
            "    debugger;\n" +
            "  }\n" +
            "  bar();\n" +
            "}\n",
    mappings: [
      // function bar() {
      {
        inputLine: 1,
        outputLine: 2
      },
      // debugger;
      {
        inputLine: 1,
        outputLine: 3
      },
      // bar();
      {
        inputLine: 1,
        outputLine: 5
      },
    ]
  },

  {
    name: "Immediately invoked function expression",
    input: "(function(){thingy()}())",
    output: "(function () {\n" +
            "  thingy()\n" +
            "}())\n"
  },

  {
    name: "Single line comment",
    input: "// Comment\n" +
           "function foo() { bar(); }\n",
    output: "// Comment\n" +
            "function foo() {\n" +
            "  bar();\n" +
            "}\n",
    mappings: [
      // // Comment
      {
        inputLine: 1,
        outputLine: 1
      }
    ]
  },

  {
    name: "Multi line comment",
    input: "/* Comment\n" +
           "more comment */\n" +
           "function foo() { bar(); }\n",
    output: "/* Comment\n" +
            "more comment */\n" +
            "function foo() {\n" +
            "  bar();\n" +
            "}\n",
    mappings: [
      // /* Comment
      {
        inputLine: 1,
        outputLine: 1
      },
      // \nmore comment */
      {
        inputLine: 1,
        outputLine: 2
      }
    ]
  },

  {
    name: "Null assignment",
    input: "var i=null;\n",
    output: "var i = null;\n",
    mappings: [
      {
        inputLine: 1,
        outputLine: 1
      }
    ]
  },

  {
    name: "Undefined assignment",
    input: "var i=undefined;\n",
    output: "var i = undefined;\n"
  },

  {
    name: "Void 0 assignment",
    input: "var i=void 0;\n",
    output: "var i = void 0;\n"
  },

  {
    name: "This property access",
    input: "var foo=this.foo;\n",
    output: "var foo = this.foo;\n"
  },

  {
    name: "True assignment",
    input: "var foo=true;\n",
    output: "var foo = true;\n"
  },

  {
    name: "False assignment",
    input: "var foo=false;\n",
    output: "var foo = false;\n"
  },

  {
    name: "For loop",
    input: "for (var i = 0; i < n; i++) { console.log(i); }",
    output: "for (var i = 0; i < n; i++) {\n" +
            "  console.log(i);\n" +
            "}\n",
    mappings: [
      // for (var i = 0; i < n; i++) {
      {
        inputLine: 1,
        outputLine: 1
      },
      // console.log(i);
      {
        inputLine: 1,
        outputLine: 2
      },
    ]
  },

  {
    name: "String with semicolon",
    input: "var foo = ';';\n",
    output: "var foo = ';';\n"
  },

  {
    name: "String with quote",
    input: "var foo = \"'\";\n",
    output: "var foo = '\\'';\n"
  },

  {
    name: "Function calls",
    input: "var result=func(a,b,c,d);",
    output: "var result = func(a, b, c, d);\n"
  },

  {
    name: "Regexp",
    input: "var r=/foobar/g;",
    output: "var r = /foobar/g;\n"
  },

  {
    name: "In operator",
    input: "if(foo in bar){doThing()}",
    output: "if (foo in bar) {\n" +
            "  doThing()\n" +
            "}\n"
  },

  {
    name: "With statement",
    input: "with(obj){crock()}",
    output: "with (obj) {\n" +
            "  crock()\n" +
            "}\n"
  },

  {
    name: "New expression",
    input: "var foo=new Foo();",
    output: "var foo = new Foo();\n"
  },

  {
    name: "Continue/break statements",
    input: "while(1){if(x){continue}if(y){break}if(z){break foo}}",
    output: "while (1) {\n" +
            "  if (x) {\n" +
            "    continue\n" +
            "  }\n" +
            "  if (y) {\n" +
            "    break\n" +
            "  }\n" +
            "  if (z) {\n" +
            "    break foo\n" +
            "  }\n" +
            "}\n"
  },

  {
    name: "Instanceof",
    input: "var a=x instanceof y;",
    output: "var a = x instanceof y;\n"
  },

  {
    name: "Binary operators",
    input: "var a=5*30;var b=5>>3;",
    output: "var a = 5 * 30;\n" +
            "var b = 5 >> 3;\n"
  },

  {
    name: "Delete",
    input: "delete obj.prop;",
    output: "delete obj.prop;\n"
  },

  {
    name: "Try/catch/finally statement",
    input: "try{dangerous()}catch(e){handle(e)}finally{cleanup()}",
    output: "try {\n" +
            "  dangerous()\n" +
            "} catch (e) {\n" +
            "  handle(e)\n" +
            "} finally {\n" +
            "  cleanup()\n" +
            "}\n"
  },

  {
    name: "If/else statement",
    input: "if(c){then()}else{other()}",
    output: "if (c) {\n" +
            "  then()\n" +
            "} else {\n" +
            "  other()\n" +
            "}\n"
  },

  {
    name: "If/else without curlies",
    input: "if(c) a else b",
    output: "if (c) a else b\n"
  },

  {
    name: "Objects",
    input: "var o={a:1,\n" +
           "       b:2};",
    output: "var o = {\n" +
            "  a: 1,\n" +
            "  b: 2\n" +
            "};\n",
    mappings: [
      // a: 1,
      {
        inputLine: 1,
        outputLine: 2
      },
      // b: 2
      {
        inputLine: 2,
        outputLine: 3
      },
    ]
  },

  {
    name: "Do/while loop",
    input: "do{x}while(y)",
    output: "do {\n" +
            "  x\n" +
            "} while (y)\n"
  },

  {
    name: "Arrays",
    input: "var a=[1,2,3];",
    output: "var a = [\n" +
            "  1,\n" +
            "  2,\n" +
            "  3\n" +
            "];\n"
  },

  {
    name: "Code that relies on ASI",
    input: "var foo = 10\n" +
           "var bar = 20\n" +
           "function g() {\n" +
           "  a()\n" +
           "  b()\n" +
           "}",
    output: "var foo = 10\n" +
            "var bar = 20\n" +
            "function g() {\n" +
            "  a()\n" +
            "  b()\n" +
            "}\n"
  },

  {
    name: "Ternary operator",
    input: "bar?baz:bang;",
    output: "bar ? baz : bang;\n"
  },

  {
    name: "Switch statements",
    input: "switch(x){case a:foo();break;default:bar()}",
    output: "switch (x) {\n" +
            "case a:\n" +
            "  foo();\n" +
            "  break;\n" +
            "default:\n" +
            "  bar()\n" +
            "}\n"
  },

  {
    name: "Multiple single line comments",
    input: "function f() {\n" +
           "  // a\n" +
           "  // b\n" +
           "  // c\n" +
           "}\n",
    output: "function f() {\n" +
            "  // a\n" +
            "  // b\n" +
            "  // c\n" +
            "}\n",
  },

  {
    name: "Indented multiline comment",
    input: "function foo() {\n" +
           "  /**\n" +
           "   * java doc style comment\n" +
           "   * more comment\n" +
           "   */\n" +
           "  bar();\n" +
           "}\n",
    output: "function foo() {\n" +
            "  /**\n" +
            "   * java doc style comment\n" +
            "   * more comment\n" +
            "   */\n" +
            "  bar();\n" +
            "}\n",
  },

  {
    name: "ASI return",
    input: "function f() {\n" +
           "  return\n" +
           "  {}\n" +
           "}\n",
    output: "function f() {\n" +
            "  return\n" +
            "  {\n" +
            "  }\n" +
            "}\n",
  },

  {
    name: "Non-ASI property access",
    input: "[1,2,3]\n" +
           "[0]",
    output: "[\n" +
            "  1,\n" +
            "  2,\n" +
            "  3\n" +
            "]\n" +
            "[0]\n"
  },

  {
    name: "Non-ASI in",
    input: "'x'\n" +
           "in foo",
    output: "'x' in foo\n"
  },

  {
    name: "Non-ASI function call",
    input: "f\n" +
           "()",
    output: "f()\n"
  },

  {
    name: "Non-ASI new",
    input: "new\n" +
           "F()",
    output: "new F()\n"
  },

  {
    name: "Getter and setter literals",
    input: "var obj={get foo(){return this._foo},set foo(v){this._foo=v}}",
    output: "var obj = {\n" +
            "  get foo() {\n" +
            "    return this._foo\n" +
            "  },\n" +
            "  set foo(v) {\n" +
            "    this._foo = v\n" +
            "  }\n" +
            "}\n"
  },

  {
    name: "Escaping backslashes in strings",
    input: "'\\\\'\n",
    output: "'\\\\'\n"
  },

  {
    name: "Escaping carriage return in strings",
    input: "'\\r'\n",
    output: "'\\r'\n"
  },

  {
    name: "Escaping tab in strings",
    input: "'\\t'\n",
    output: "'\\t'\n"
  },

  {
    name: "Escaping vertical tab in strings",
    input: "'\\v'\n",
    output: "'\\v'\n"
  },

  {
    name: "Escaping form feed in strings",
    input: "'\\f'\n",
    output: "'\\f'\n"
  },

  {
    name: "Escaping null character in strings",
    input: "'\\0'\n",
    output: "'\\0'\n"
  },

];

var sourceMap = this.sourceMap || require("source-map");

function run_test() {
  testCases.forEach(function (test) {
    console.log(test.name);

    var actual = prettyFast(test.input, {
      indent: "  ",
      url: "test.js"
    });

    if (actual.code !== test.output) {
      throw new Error("Expected:\n" + test.output
                      + "\nGot:\n" + actual.code);
    }

    if (test.mappings) {
      var smc = new sourceMap.SourceMapConsumer(actual.map.toJSON());
      test.mappings.forEach(function (m) {
        var query = { line: m.outputLine, column: 0 };
        var original = smc.originalPositionFor(query);
        if (original.line != m.inputLine) {
          throw new Error("Querying:\n" + JSON.stringify(query, null, 2) + "\n"
                          + "Expected line:\n" + m.inputLine + "\n"
                          + "Got:\n" + JSON.stringify(original, null, 2));
        }
      });
    }
  });
  console.log("✓ All tests pass!");
}

// Only run the tests if this is node and we are running this file
// directly. (Firefox's test runner will import this test file, and then call
// run_test itself.)
if (typeof require == "function" && typeof module == "object"
    && require.main === module) {
  run_test();
}
