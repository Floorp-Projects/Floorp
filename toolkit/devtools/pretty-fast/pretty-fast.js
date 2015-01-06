/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/*
 * Copyright 2013 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.md or:
 * http://opensource.org/licenses/BSD-2-Clause
 */
(function (root, factory) {
  "use strict";

  if (typeof define === "function" && define.amd) {
    define(factory);
  } else if (typeof exports === "object") {
    module.exports = factory();
  } else {
    root.prettyFast = factory();
  }
}(this, function () {
  "use strict";

  var acorn = this.acorn || require("acorn/acorn");
  var sourceMap = this.sourceMap || require("source-map");
  var SourceNode = sourceMap.SourceNode;

  // If any of these tokens are seen before a "[" token, we know that "[" token
  // is the start of an array literal, rather than a property access.
  //
  // The only exception is "}", which would need to be disambiguated by
  // parsing. The majority of the time, an open bracket following a closing
  // curly is going to be an array literal, so we brush the complication under
  // the rug, and handle the ambiguity by always assuming that it will be an
  // array literal.
  var PRE_ARRAY_LITERAL_TOKENS = {
    "typeof": true,
    "void": true,
    "delete": true,
    "case": true,
    "do": true,
    "=": true,
    "in": true,
    "{": true,
    "*": true,
    "/": true,
    "%": true,
    "else": true,
    ";": true,
    "++": true,
    "--": true,
    "+": true,
    "-": true,
    "~": true,
    "!": true,
    ":": true,
    "?": true,
    ">>": true,
    ">>>": true,
    "<<": true,
    "||": true,
    "&&": true,
    "<": true,
    ">": true,
    "<=": true,
    ">=": true,
    "instanceof": true,
    "&": true,
    "^": true,
    "|": true,
    "==": true,
    "!=": true,
    "===": true,
    "!==": true,
    ",": true,

    "}": true
  };

  /**
   * Determines if we think that the given token starts an array literal.
   *
   * @param Object token
   *        The token we want to determine if it is an array literal.
   * @param Object lastToken
   *        The last token we added to the pretty printed results.
   *
   * @returns Boolean
   *          True if we believe it is an array literal, false otherwise.
   */
  function isArrayLiteral(token, lastToken) {
    if (token.type.type != "[") {
      return false;
    }
    if (!lastToken) {
      return true;
    }
    if (lastToken.type.isAssign) {
      return true;
    }
    return !!PRE_ARRAY_LITERAL_TOKENS[
      lastToken.type.keyword || lastToken.type.type
    ];
  }

  // If any of these tokens are followed by a token on a new line, we know that
  // ASI cannot happen.
  var PREVENT_ASI_AFTER_TOKENS = {
    // Binary operators
    "*": true,
    "/": true,
    "%": true,
    "+": true,
    "-": true,
    "<<": true,
    ">>": true,
    ">>>": true,
    "<": true,
    ">": true,
    "<=": true,
    ">=": true,
    "instanceof": true,
    "in": true,
    "==": true,
    "!=": true,
    "===": true,
    "!==": true,
    "&": true,
    "^": true,
    "|": true,
    "&&": true,
    "||": true,
    ",": true,
    ".": true,
    "=": true,
    "*=": true,
    "/=": true,
    "%=": true,
    "+=": true,
    "-=": true,
    "<<=": true,
    ">>=": true,
    ">>>=": true,
    "&=": true,
    "^=": true,
    "|=": true,
    // Unary operators
    "delete": true,
    "void": true,
    "typeof": true,
    "~": true,
    "!": true,
    "new": true,
    // Function calls and grouped expressions
    "(": true
  };

  // If any of these tokens are on a line after the token before it, we know
  // that ASI cannot happen.
  var PREVENT_ASI_BEFORE_TOKENS = {
    // Binary operators
    "*": true,
    "/": true,
    "%": true,
    "<<": true,
    ">>": true,
    ">>>": true,
    "<": true,
    ">": true,
    "<=": true,
    ">=": true,
    "instanceof": true,
    "in": true,
    "==": true,
    "!=": true,
    "===": true,
    "!==": true,
    "&": true,
    "^": true,
    "|": true,
    "&&": true,
    "||": true,
    ",": true,
    ".": true,
    "=": true,
    "*=": true,
    "/=": true,
    "%=": true,
    "+=": true,
    "-=": true,
    "<<=": true,
    ">>=": true,
    ">>>=": true,
    "&=": true,
    "^=": true,
    "|=": true,
    // Function calls
    "(": true
  };

  /**
   * Determines if Automatic Semicolon Insertion (ASI) occurs between these
   * tokens.
   *
   * @param Object token
   *        The current token.
   * @param Object lastToken
   *        The last token we added to the pretty printed results.
   *
   * @returns Boolean
   *          True if we believe ASI occurs.
   */
  function isASI(token, lastToken) {
    if (!lastToken) {
      return false;
    }
    if (token.startLoc.line === lastToken.startLoc.line) {
      return false;
    }
    if (PREVENT_ASI_AFTER_TOKENS[
      lastToken.type.type || lastToken.type.keyword
    ]) {
      return false;
    }
    if (PREVENT_ASI_BEFORE_TOKENS[token.type.type || token.type.keyword]) {
      return false;
    }
    return true;
  }

  /**
   * Determine if we have encountered a getter or setter.
   *
   * @param Object token
   *        The current token. If this is a getter or setter, it would be the
   *        property name.
   * @param Object lastToken
   *        The last token we added to the pretty printed results. If this is a
   *        getter or setter, it would be the `get` or `set` keyword
   *        respectively.
   * @param Array stack
   *        The stack of open parens/curlies/brackets/etc.
   *
   * @returns Boolean
   *          True if this is a getter or setter.
   */
  function isGetterOrSetter(token, lastToken, stack) {
    return stack[stack.length - 1] == "{"
      && lastToken
      && lastToken.type.type == "name"
      && (lastToken.value == "get" || lastToken.value == "set")
      && token.type.type == "name";
  }

  /**
   * Determine if we should add a newline after the given token.
   *
   * @param Object token
   *        The token we are looking at.
   * @param Array stack
   *        The stack of open parens/curlies/brackets/etc.
   *
   * @returns Boolean
   *          True if we should add a newline.
   */
  function isLineDelimiter(token, stack) {
    if (token.isArrayLiteral) {
      return true;
    }
    var ttt = token.type.type;
    var top = stack[stack.length - 1];
    return ttt == ";" && top != "("
      || ttt == "{"
      || ttt == "," && top != "("
      || ttt == ":" && (top == "case" || top == "default");
  }

  /**
   * Append the necessary whitespace to the result after we have added the given
   * token.
   *
   * @param Object token
   *        The token that was just added to the result.
   * @param Function write
   *        The function to write to the pretty printed results.
   * @param Array stack
   *        The stack of open parens/curlies/brackets/etc.
   *
   * @returns Boolean
   *          Returns true if we added a newline to result, false in all other
   *          cases.
   */
  function appendNewline(token, write, stack) {
    if (isLineDelimiter(token, stack)) {
      write("\n", token.startLoc.line, token.startLoc.column);
      return true;
    }
    return false;
  }

  /**
   * Determines if we need to add a space between the last token we added and
   * the token we are about to add.
   *
   * @param Object token
   *        The token we are about to add to the pretty printed code.
   * @param Object lastToken
   *        The last token added to the pretty printed code.
   */
  function needsSpaceAfter(token, lastToken) {
    if (lastToken) {
      if (lastToken.type.isLoop) {
        return true;
      }
      if (lastToken.type.isAssign) {
        return true;
      }
      if (lastToken.type.binop != null) {
        return true;
      }

      var ltt = lastToken.type.type;
      if (ltt == "?") {
        return true;
      }
      if (ltt == ":") {
        return true;
      }
      if (ltt == ",") {
        return true;
      }
      if (ltt == ";") {
        return true;
      }

      var ltk = lastToken.type.keyword;
      if (ltk != null) {
        if (ltk == "break" || ltk == "continue" || ltk == "return") {
          return token.type.type != ";";
        }
        if (ltk != "debugger"
            && ltk != "null"
            && ltk != "true"
            && ltk != "false"
            && ltk != "this"
            && ltk != "default") {
          return true;
        }
      }

      if (ltt == ")" && (token.type.type != ")"
                         && token.type.type != "]"
                         && token.type.type != ";"
                         && token.type.type != ","
                         && token.type.type != ".")) {
        return true;
      }
    }

    if (token.type.isAssign) {
      return true;
    }
    if (token.type.binop != null) {
      return true;
    }
    if (token.type.type == "?") {
      return true;
    }

    return false;
  }

  /**
   * Add the required whitespace before this token, whether that is a single
   * space, newline, and/or the indent on fresh lines.
   *
   * @param Object token
   *        The token we are about to add to the pretty printed code.
   * @param Object lastToken
   *        The last token we added to the pretty printed code.
   * @param Boolean addedNewline
   *        Whether we added a newline after adding the last token to the pretty
   *        printed code.
   * @param Function write
   *        The function to write pretty printed code to the result SourceNode.
   * @param Object options
   *        The options object.
   * @param Number indentLevel
   *        The number of indents deep we are.
   * @param Array stack
   *        The stack of open curlies, brackets, etc.
   */
  function prependWhiteSpace(token, lastToken, addedNewline, write, options,
                             indentLevel, stack) {
    var ttk = token.type.keyword;
    var ttt = token.type.type;
    var newlineAdded = addedNewline;
    var ltt = lastToken ? lastToken.type.type : null;

    // Handle whitespace and newlines after "}" here instead of in
    // `isLineDelimiter` because it is only a line delimiter some of the
    // time. For example, we don't want to put "else if" on a new line after
    // the first if's block.
    if (lastToken && ltt == "}") {
      if (ttk == "while" && stack[stack.length - 1] == "do") {
        write(" ",
              lastToken.startLoc.line,
              lastToken.startLoc.column);
      } else if (ttk == "else" ||
                 ttk == "catch" ||
                 ttk == "finally") {
        write(" ",
              lastToken.startLoc.line,
              lastToken.startLoc.column);
      } else if (ttt != "(" &&
                 ttt != ";" &&
                 ttt != "," &&
                 ttt != ")" &&
                 ttt != ".") {
        write("\n",
              lastToken.startLoc.line,
              lastToken.startLoc.column);
        newlineAdded = true;
      }
    }

    if (isGetterOrSetter(token, lastToken, stack)) {
      write(" ",
            lastToken.startLoc.line,
            lastToken.startLoc.column);
    }

    if (ttt == ":" && stack[stack.length - 1] == "?") {
      write(" ",
            lastToken.startLoc.line,
            lastToken.startLoc.column);
    }

    if (lastToken && ltt != "}" && ttk == "else") {
      write(" ",
            lastToken.startLoc.line,
            lastToken.startLoc.column);
    }

    function ensureNewline() {
      if (!newlineAdded) {
        write("\n",
              lastToken.startLoc.line,
              lastToken.startLoc.column);
        newlineAdded = true;
      }
    }

    if (isASI(token, lastToken)) {
      ensureNewline();
    }

    if (decrementsIndent(ttt, stack)) {
      ensureNewline();
    }

    if (newlineAdded) {
      if (ttk == "case" || ttk == "default") {
        write(repeat(options.indent, indentLevel - 1),
              token.startLoc.line,
              token.startLoc.column);
      } else {
        write(repeat(options.indent, indentLevel),
              token.startLoc.line,
              token.startLoc.column);
      }
    } else if (needsSpaceAfter(token, lastToken)) {
      write(" ",
            lastToken.startLoc.line,
            lastToken.startLoc.column);
    }
  }

  /**
   * Repeat the `str` string `n` times.
   *
   * @param String str
   *        The string to be repeated.
   * @param Number n
   *        The number of times to repeat the string.
   *
   * @returns String
   *          The repeated string.
   */
  function repeat(str, n) {
    var result = "";
    while (n > 0) {
      if (n & 1) {
        result += str;
      }
      n >>= 1;
      str += str;
    }
    return result;
  }

  /**
   * Make sure that we output the escaped character combination inside string
   * literals instead of various problematic characters.
   */
  var sanitize = (function () {
    var escapeCharacters = {
      // Backslash
      "\\": "\\\\",
      // Newlines
      "\n": "\\n",
      // Carriage return
      "\r": "\\r",
      // Tab
      "\t": "\\t",
      // Vertical tab
      "\v": "\\v",
      // Form feed
      "\f": "\\f",
      // Null character
      "\0": "\\0",
      // Single quotes
      "'": "\\'"
    };

    var regExpString = "("
      + Object.keys(escapeCharacters)
              .map(function (c) { return escapeCharacters[c]; })
              .join("|")
      + ")";
    var escapeCharactersRegExp = new RegExp(regExpString, "g");

    return function (str) {
      return str.replace(escapeCharactersRegExp, function (_, c) {
        return escapeCharacters[c];
      });
    };
  }());
  /**
   * Add the given token to the pretty printed results.
   *
   * @param Object token
   *        The token to add.
   * @param Function write
   *        The function to write pretty printed code to the result SourceNode.
   */
  function addToken(token, write) {
    if (token.type.type == "string") {
      write("'" + sanitize(token.value) + "'",
            token.startLoc.line,
            token.startLoc.column);
    } else if (token.type.type == "regexp") {
      write(String(token.value.value),
            token.startLoc.line,
            token.startLoc.column);
    } else {
      write(String(token.value != null ? token.value : token.type.type),
            token.startLoc.line,
            token.startLoc.column);
    }
  }

  /**
   * Returns true if the given token type belongs on the stack.
   */
  function belongsOnStack(token) {
    var ttt = token.type.type;
    var ttk = token.type.keyword;
    return ttt == "{"
      || ttt == "("
      || ttt == "["
      || ttt == "?"
      || ttk == "do"
      || ttk == "switch"
      || ttk == "case"
      || ttk == "default";
  }

  /**
   * Returns true if the given token should cause us to pop the stack.
   */
  function shouldStackPop(token, stack) {
    var ttt = token.type.type;
    var ttk = token.type.keyword;
    var top = stack[stack.length - 1];
    return ttt == "]"
      || ttt == ")"
      || ttt == "}"
      || (ttt == ":" && (top == "case" || top == "default" || top == "?"))
      || (ttk == "while" && top == "do");
  }

  /**
   * Returns true if the given token type should cause us to decrement the
   * indent level.
   */
  function decrementsIndent(tokenType, stack) {
    return tokenType == "}"
      || (tokenType == "]" && stack[stack.length - 1] == "[\n");
  }

  /**
   * Returns true if the given token should cause us to increment the indent
   * level.
   */
  function incrementsIndent(token) {
    return token.type.type == "{"
      || token.isArrayLiteral
      || token.type.keyword == "switch";
  }

  /**
   * Add a comment to the pretty printed code.
   *
   * @param Function write
   *        The function to write pretty printed code to the result SourceNode.
   * @param Number indentLevel
   *        The number of indents deep we are.
   * @param Object options
   *        The options object.
   * @param Boolean block
   *        True if the comment is a multiline block style comment.
   * @param String text
   *        The text of the comment.
   * @param Number line
   *        The line number to comment appeared on.
   * @param Number column
   *        The column number the comment appeared on.
   */
  function addComment(write, indentLevel, options, block, text, line, column) {
    var indentString = repeat(options.indent, indentLevel);

    write(indentString, line, column);
    if (block) {
      write("/*");
      write(text
            .split(new RegExp("/\n" + indentString + "/", "g"))
            .join("\n" + indentString));
      write("*/");
    } else {
      write("//");
      write(text);
    }
    write("\n");
  }

  /**
   * The main function.
   *
   * @param String input
   *        The ugly JS code we want to pretty print.
   * @param Object options
   *        The options object. Provides configurability of the pretty
   *        printing. Properties:
   *          - url: The URL string of the ugly JS code.
   *          - indent: The string to indent code by.
   *
   * @returns Object
   *          An object with the following properties:
   *            - code: The pretty printed code string.
   *            - map: A SourceMapGenerator instance.
   */
  return function prettyFast(input, options) {
    // The level of indents deep we are.
    var indentLevel = 0;

    // We will accumulate the pretty printed code in this SourceNode.
    var result = new SourceNode();

    /**
     * Write a pretty printed string to the result SourceNode.
     *
     * We buffer our writes so that we only create one mapping for each line in
     * the source map. This enhances performance by avoiding extraneous mapping
     * serialization, and flattening the tree that
     * `SourceNode#toStringWithSourceMap` will have to recursively walk. When
     * timing how long it takes to pretty print jQuery, this optimization
     * brought the time down from ~390 ms to ~190ms!
     *
     * @param String str
     *        The string to be added to the result.
     * @param Number line
     *        The line number the string came from in the ugly source.
     * @param Number column
     *        The column number the string came from in the ugly source.
     */
    var write = (function () {
      var buffer = [];
      var bufferLine = -1;
      var bufferColumn = -1;
      return function write(str, line, column) {
        if (line != null && bufferLine === -1) {
          bufferLine = line;
        }
        if (column != null && bufferColumn === -1) {
          bufferColumn = column;
        }
        buffer.push(str);

        if (str == "\n") {
          var lineStr = "";
          for (var i = 0, len = buffer.length; i < len; i++) {
            lineStr += buffer[i];
          }
          result.add(new SourceNode(bufferLine, bufferColumn, options.url,
                                    lineStr));
          buffer.splice(0, buffer.length);
          bufferLine = -1;
          bufferColumn = -1;
        }
      };
    }());

    // Whether or not we added a newline on after we added the last token.
    var addedNewline = false;

    // The current token we will be adding to the pretty printed code.
    var token;

    // Shorthand for token.type.type, so we don't have to repeatedly access
    // properties.
    var ttt;

    // Shorthand for token.type.keyword, so we don't have to repeatedly access
    // properties.
    var ttk;

    // The last token we added to the pretty printed code.
    var lastToken;

    // Stack of token types/keywords that can affect whether we want to add a
    // newline or a space. We can make that decision based on what token type is
    // on the top of the stack. For example, a comma in a parameter list should
    // be followed by a space, while a comma in an object literal should be
    // followed by a newline.
    //
    // Strings that go on the stack:
    //
    //   - "{"
    //   - "("
    //   - "["
    //   - "[\n"
    //   - "do"
    //   - "?"
    //   - "switch"
    //   - "case"
    //   - "default"
    //
    // The difference between "[" and "[\n" is that "[\n" is used when we are
    // treating "[" and "]" tokens as line delimiters and should increment and
    // decrement the indent level when we find them.
    var stack = [];

    // Acorn's tokenizer will always yield comments *before* the token they
    // follow (unless the very first thing in the source is a comment), so we
    // have to queue the comments in order to pretty print them in the correct
    // location. For example, the source file:
    //
    //     foo
    //     // a
    //     // b
    //     bar
    //
    // When tokenized by acorn, gives us the following token stream:
    //
    //     [ '// a', '// b', foo, bar ]
    var commentQueue = [];

    var getToken = acorn.tokenize(input, {
      locations: true,
      sourceFile: options.url,
      onComment: function (block, text, start, end, startLoc, endLoc) {
        if (lastToken) {
          commentQueue.push({
            block: block,
            text: text,
            line: startLoc.line,
            column: startLoc.column,
            trailing: lastToken.endLoc.line == startLoc.line
          });
        } else {
          addComment(write, indentLevel, options, block, text, startLoc.line,
                     startLoc.column);
          addedNewline = true;
        }
      }
    });

    for (;;) {
      token = getToken();

      ttk = token.type.keyword;
      ttt = token.type.type;

      if (ttt == "eof") {
        if (!addedNewline) {
          write("\n");
        }
        break;
      }

      token.isArrayLiteral = isArrayLiteral(token, lastToken);

      if (belongsOnStack(token)) {
        if (token.isArrayLiteral) {
          stack.push("[\n");
        } else {
          stack.push(ttt || ttk);
        }
      }

      if (decrementsIndent(ttt, stack)) {
        indentLevel--;
        if (ttt == "}"
            && stack.length > 1
            && stack[stack.length - 2] == "switch") {
          indentLevel--;
        }
      }

      prependWhiteSpace(token, lastToken, addedNewline, write, options,
                        indentLevel, stack);
      addToken(token, write);
      if (commentQueue.length === 0 || !commentQueue[0].trailing) {
        addedNewline = appendNewline(token, write, stack);
      }

      if (shouldStackPop(token, stack)) {
        stack.pop();
        if (token == "}" && stack.length
            && stack[stack.length - 1] == "switch") {
          stack.pop();
        }
      }

      if (incrementsIndent(token)) {
        indentLevel++;
      }

      // Acorn's tokenizer re-uses tokens, so we have to copy the last token on
      // every iteration. We follow acorn's lead here, and reuse the lastToken
      // object the same way that acorn reuses the token object. This allows us
      // to avoid allocations and minimize GC pauses.
      if (!lastToken) {
        lastToken = { startLoc: {}, endLoc: {} };
      }
      lastToken.start = token.start;
      lastToken.end = token.end;
      lastToken.startLoc.line = token.startLoc.line;
      lastToken.startLoc.column = token.startLoc.column;
      lastToken.endLoc.line = token.endLoc.line;
      lastToken.endLoc.column = token.endLoc.column;
      lastToken.type = token.type;
      lastToken.value = token.value;
      lastToken.isArrayLiteral = token.isArrayLiteral;

      // Apply all the comments that have been queued up.
      if (commentQueue.length) {
        if (!addedNewline && !commentQueue[0].trailing) {
          write("\n");
        }
        if (commentQueue[0].trailing) {
          write(" ");
        }
        for (var i = 0, n = commentQueue.length; i < n; i++) {
          var comment = commentQueue[i];
          var commentIndentLevel = commentQueue[i].trailing ? 0 : indentLevel;
          addComment(write, commentIndentLevel, options, comment.block,
                     comment.text, comment.line, comment.column);
        }
        addedNewline = true;
        commentQueue.splice(0, commentQueue.length);
      }
    }

    return result.toStringWithSourceMap({ file: options.url });
  };

}.bind(this)));
