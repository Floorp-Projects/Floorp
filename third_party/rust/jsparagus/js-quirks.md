## JS syntactic quirks

JavaScript is rather hard to parse. Here is an in-depth accounting of
its syntactic quirks, with an eye toward actually implementing a parser
from scratch.

With apologies to the generous people who work on the standard. Thanks
for doing that—better you than me.

Thanks to @mathiasbynens for pointing out an additional quirk.

Problems are rated in terms of difficulty, from `(*)` = easy to `(***)`
= hard. We’ll start with the easiest problems.


### Dangling else (*)

If you know what this is, you may be excused.

Statements like `if (EXPR) STMT if (EXPR) STMT else STMT`
are straight-up ambiguous in the JS formal grammar.
The ambiguity is resolved with
[a line of specification text](https://tc39.es/ecma262/#sec-if-statement):

> Each `else` for which the choice of associated `if` is ambiguous shall
> be associated with the nearest possible `if` that would otherwise have
> no corresponding `else`.

I love this sentence. Something about it cracks me up, I dunno...

In a recursive descent parser, just doing the dumbest possible thing
correctly implements this rule.

A parser generator has to decide what to do. In Yacc, you can use
operator precedence for this.

Yacc aside: This should seem a little outrageous at first, as `else` is
hardly an operator. It helps if you understand what Yacc is doing. In LR
parsers, this kind of ambiguity in the grammar manifests as a
shift-reduce conflict. In this case, when we’ve already parsed `if (
Expression ) Statement if ( Expression ) Statement`
and are looking at `else`, it’s unclear to Yacc
whether to reduce the if-statement or shift `else`. Yacc does not offer
a feature that lets us just say "always shift `else` here"; but there
*is* a Yacc feature that lets us resolve shift-reduce conflicts in a
rather odd, indirect way: operator precedence.  We can resolve this
conflict by making `else` higher-precedence than the preceding symbol
`)`.

Alternatively, I believe it’s equivalent to add "[lookahead ≠ `else`]"
at the end of the IfStatement production that doesn’t have an `else`.


### Other ambiguities and informal parts of the spec (*)

Not all of the spec is as formal as it seems at first. Most of the stuff
in this section is easy to deal with, but #4 is special.

1.  The lexical grammar is ambiguous: when looking at the characters `<<=`,
    there is the question of whether to parse that as one token `<<=`, two
    tokens (`< <=` or `<< =`), or three (`< < =`).

    Of course every programming language has this, and the fix is one
    sentence of prose in the spec:

    > The source text is scanned from left to right, repeatedly taking the
    > longest possible sequence of code points as the next input element.

    This is easy enough for hand-coded lexers, and for systems that are
    designed to use separate lexical and syntactic grammars.  (Other
    parser generators may need help to avoid parsing `functionf(){}` as
    a function.)

2.  The above line of prose does not apply *within* input elements, in
    components of the lexical grammar. In those cases, the same basic
    idea ("maximum munch") is specified using lookahead restrictions at
    the end of productions:

    > *LineTerminatorSequence* ::  
    > &nbsp; &nbsp; &lt;LF&gt;  
    > &nbsp; &nbsp; &lt;CR&gt;[lookahead &ne; &lt;LF&gt;]  
    > &nbsp; &nbsp; &lt;LS&gt;  
    > &nbsp; &nbsp; &lt;PS&gt;  
    > &nbsp; &nbsp; &lt;CR&gt;&lt;LF&gt;

    The lookahead restriction prevents a CR LF sequence from being
    parsed as two adjacent *LineTerminatorSequence*s.

    This technique is used in several places, particularly in
    [*NotEscapeSequences*](https://tc39.es/ecma262/#prod-NotEscapeSequence).

3.  Annex B.1.4 extends the syntax for regular expressions, making the
    grammar ambiguous. Again, a line of prose explains how to cope:

    > These changes introduce ambiguities that are broken by the
    > ordering of grammar productions and by contextual
    > information. When parsing using the following grammar, each
    > alternative is considered only if previous production alternatives
    > do not match.

4.  Annex B.1.2 extends the syntax of string literals to allow legacy
    octal escape sequences, like `\033`. It says:

    > The syntax and semantics of 11.8.4 is extended as follows except
    > that this extension is not allowed for strict mode code:

    ...followed by a new definition of *EscapeSequence*.

    So there are two sets of productions for *EscapeSequence*, and an
    implementation is required to implement both and dynamically switch
    between them.

    This means that `function f() { "\033"; "use strict"; }` is a
    SyntaxError, even though the octal escape is scanned before we know
    we're in strict mode.

For another ambiguity, see "Slashes" below.


### Unicode quirks

JavaScript source is Unicode and usually follows Unicode rules for thing
like identifiers and whitespace, but it has a few special cases: `$`,
`_`, `U+200C ZERO WIDTH NON-JOINER`, and `U+200D ZERO WIDTH JOINER` are
legal in identifiers (the latter two only after the first character), and
`U+FEFF ZERO WIDTH NO-BREAK SPACE` (also known as the byte-order mark) is
treated as whitespace.

It also allows any code point, including surrogate halves, even though the
Unicode standard says that unpaired surrogate halves should be treated as
encoding errors.


### Legacy octal literals and escape sequences (*)

This is more funny than difficult.

In a browser, in non-strict code, every sequence of decimal digits (not
followed by an identifier character) is a *NumericLiteral* token.

If it starts with `0`, with more digits after, then it's a legacy Annex
B.1.1 literal. If the token contains an `8` or a `9`, it's a decimal
number. Otherwise, hilariously, it's octal.

```
js> [067, 068, 069, 070]
[55, 68, 69, 56]
```

There are also legacy octal escape sequences in strings, and these have
their own quirks. `'\07' === '\u{7}'`, but `'\08' !== '\u{8}'` since 8
is not an octal digit. Instead `'\08' === '\0' + '8'`, because `\0`
followed by `8` or `9` is a legacy octal escape sequence representing
the null character. (Not to be confused with `\0` in strict code, not
followed by a digit, which still represents the null character, but
doesn't count as octal.)

None of this is hard to implement, but figuring out what the spec says
is hard.


### Strict mode (*)

*(entangled with: lazy compilation)*

A script or function can start with this:

```js
"use strict";
```

This enables ["strict mode"](https://tc39.es/ecma262/#sec-strict-mode-of-ecmascript).
Additionally, all classes and modules are strict mode code.

Strict mode has both parse-time and run-time effects. Parse-time effects
include:

*   Strict mode affects the lexical grammar: octal integer literals are
    SyntaxErrors, octal character escapes are SyntaxErrors, and a
    handful of words like `private` and `interface` are reserved (and
    thus usually SyntaxErrors) in strict mode.

    Like the situation with slashes, this means it is not possible to
    implement a complete lexer for JS without also parsing—at least
    enough to detect class boundaries, "use strict" directives in
    functions, and function boundaries.

*   It’s a SyntaxError to have bindings named `eval` or `arguments` in
    strict mode code, or to assign to `eval` or `arguments`.

*   It’s a SyntaxError to have two argument bindings with the same name
    in a strict function.

    Interestingly, you don’t always know if you’re in strict mode or not
    when parsing arguments.

    ```js
    function foo(a, a) {
        "use strict";
    }
    ```

    When the implementation reaches the Use Strict Directive, it must
    either know that `foo` has two arguments both named `a`, or switch
    to strict mode, go back, and reparse the function from the
    beginning.

    Fortunately an Early Error rule prohibits mixing `"use strict"` with
    more complex parameter lists, like `function foo(x = eval('')) {`.

*   The expression syntax “`delete` *Identifier*” and the abominable
    *WithStatement* are banned in strict mode.


### Conditional keywords (**)

In some programming languages, you could write a lexer that has rules
like

*   When you see `if`, return `Token::If`.

*   When you see something like `apple` or `arrow` or `target`,
    return `Token::Identifier`.

Not so in JavaScript. The input `if` matches both the terminal `if` and
the nonterminal *IdentifierName*, both of which appear in the high-level
grammar. The same goes for `target`.

This poses a deceptively difficult problem for table-driven parsers.
Such parsers run on a stream of token-ids, but the question of which
token-id to use for a word like `if` or `target` is ambiguous. The
current parser state can't fully resolve the ambiguity: there are cases
like `class C { get` where the token `get` might match either as a
keyword (the start of a getter) or as an *IdentifierName* (a method or
property named `get`) in different grammatical productions.

All keywords are conditional, but some are more conditional than others.
The rules are inconsistent to a tragicomic extent. Keywords like `if`
that date back to JavaScript 1.0 are always keywords except when used as
property names or method names. They can't be variable names. Two
conditional keywords (`await` and `yield`) are in the *Keyword* list;
the rest are not. New syntax that happened to be introduced around the
same time as strict mode was awarded keyword status in strict mode. The
rules are scattered through the spec. All this interacts with `\u0065`
Unicode escape sequences somehow. It’s just unbelievably confusing.

(After writing this section, I
[proposed revisions to the specification](https://github.com/tc39/ecma262/pull/1694)
to make it a little less confusing.)

*   Thirty-six words are always reserved:

    > `break` `case` `catch` `class` `const` `continue` `debugger`
    > `default` `delete` `do` `else` `enum` `export` `extends` `false`
    > `finally` `for` `function` `if` `import` `in` `instanceof` `new`
    > `null` `return` `super` `switch` `this` `throw` `true` `try`
    > `typeof` `var` `void` `while` `with`

    These tokens can't be used as names of variables or arguments.
    They're always considered special *except* when used as property
    names, method names, or import/export names in modules.

    ```js
    // property names
    let obj = {if: 3, function: 4};
    assert(obj.if == 3);

    // method names
    class C {
      if() {}
      function() {}
    }

    // imports and exports
    import {if as my_if} from "modulename";
    export {my_if as if};
    ```

*   Two more words, `yield` and `await`, are in the *Keyword* list but
    do not always act like keywords in practice.

    *   `yield` is a *Keyword*; but it can be used as an identifier,
        except in generators and strict mode code.

        This means that `yield - 1` is valid both inside and outside
        generators, with different meanings. Outside a generator, it’s
        subtraction. Inside, it yields the value **-1**.

        That reminds me of the Groucho Marx line: Outside of a dog, a
        book is a man’s best friend. Inside of a dog it’s too dark to
        read.

    *   `await` is like that, but in async functions. Also it’s not a
        valid identifier in modules.

    Conditional keywords are entangled with slashes: `yield /a/g` is two
    tokens in a generator but five tokens elsewhere.

*   In strict mode code, `implements`, `interface`, `package`,
    `private`, `protected`, and `public` are reserved (via Early Errors
    rules).

    This is reflected in the message and location information for
    certain syntax errors:

    ```
    SyntaxError: implements is a reserved identifier:
    class implements {}
    ......^

    SyntaxError: implements is a reserved identifier:
    function implements() { "use strict"; }
    ....................................^
    ```

*   `let` is not a *Keyword* or *ReservedWord*. Usually it can be an
    identifier. It is special at the beginning of a statement or after
    `for (` or `for await (`.

    ```js
    var let = [new Date];   // ok: let as identifier
    let v = let;            // ok: let as keyword, then identifier
    let let;           // SyntaxError: banned by special early error rule
    let.length;        // ok: `let .` -> ExpressionStatement
    let[0].getYear();  // SyntaxError: `let [` -> LexicalDeclaration
    ```

    In strict mode code, `let` is reserved.

*   `static` is similar. It’s a valid identifier, except in strict
    mode. It’s only special at the beginning of a *ClassElement*.

    In strict mode code, `static` is reserved.

*   `async` is similar, but trickier. It’s an identifier. It is special
    only if it’s marking an `async` function, method, or arrow function
    (the tough case, since you won’t know it’s an arrow function until
    you see the `=>`, possibly much later).

    ```js
    function async() {}   // normal function named "async"

    async();        // ok, `async` is an Identifier; function call
    async() => {};  // ok, `async` is not an Identifier; async arrow function
    ```

*   `of` is special only in one specific place in `for-of` loop syntax.

    ```js
    var of = [1, 2, 3];
    for (of of of) console.log(of);  // logs 1, 2, 3
    ```

    Amazingly, both of the following are valid JS code:

    ```js
    for (async of => {};;) {}
    for (async of []) {}
    ```

    In the first line, `async` is a keyword and `of` is an identifier;
    in the second line it's the other way round.

    Even a simplified JS grammar can't be LR(1) as long as it includes
    the features used here!

*   `get` and `set` are special only in a class or an object literal,
    and then only if followed by a PropertyName:

    ```js
    var obj1 = {get: f};      // `get` is an identifier
    var obj2 = {get x() {}};  // `get` means getter

    class C1 { get = 3; }     // `get` is an identifier
    class C2 { get x() {} }   // `get` means getter
    ```

*   `target` is special only in `new.target`.

*   `arguments` and `eval` can't be binding names, and can't be assigned
    to, in strict mode code.

To complicate matters, there are a few grammatical contexts where both
*IdentifierName* and *Identifier* match. For example, after `var {`
there are two possibilities:

```js
// Longhand properties: BindingProperty -> PropertyName -> IdentifierName
var { xy: v } = obj;    // ok
var { if: v } = obj;    // ok, `if` is an IdentifierName

// Shorthand properties: BindingProperty -> SingleNameBinding -> BindingIdentifier -> Identifier
var { xy } = obj;       // ok
var { if } = obj;       // SyntaxError: `if` is not an Identifier
```


### Escape sequences in keywords

*(entangled with: conditional keywords, ASI)*

You can use escape sequences to write variable and property names, but
not keywords (including contextual keywords in contexts where they act
as keywords).

So `if (foo) {}` and `{ i\f: 0 }` are legal but `i\u0066 (foo)` is not.

And you don't necessarily know if you're lexing a contextual keyword
until the next token: `({ g\u0065t: 0 })` is legal, but
`({ g\u0065t x(){} })` is not.

And for `let` it's even worse: `l\u0065t` by itself is a legal way to
reference a variable named `let`, which means that

```js
let
x
```
declares a variable named `x`, while, thanks to ASI,

```js
l\u0065t
x
```
is a reference to a variable named `let` followed by a reference to a
variable named `x`.


### Early errors (**)

*(entangled with: lazy parsing, conditional keywords, ASI)*

Some early errors are basically syntactic. Others are not.

This is entangled with lazy compilation: "early errors" often involve a
retrospective look at an arbitrarily large glob of code we just parsed,
but in Beast Mode we’re not building an AST. In fact we would like to be
doing as little bookkeeping as possible.

Even setting that aside, every early error is a special case, and it’s
just a ton of rules that all have to be implemented by hand.

Here are some examples of Early Error rules—setting aside restrictions
that are covered adequately elsewhere:

*   Rules about names:

    *   Rules that affect the set of keywords (character sequences that
        match *IdentifierName* but are not allowed as binding names) based
        on whether or not we’re in strict mode code, or in a
        *Module*. Affected identifiers include `arguments`, `eval`, `yield`,
        `await`, `let`, `implements`, `interface`, `package`, `private`,
        `protected`, `public`, `static`.

        *   One of these is a strangely worded rule which prohibits using
            `yield` as a *BindingIdentifier*.  At first blush, this seems
            like it could be enforced in the grammar, but that approach
            would make this a valid program, due to ASI:

            ```js
            let
            yield 0;
            ```

            Enforcing the same rule using an Early Error prohibits ASI here.
            It works by exploiting the detailed inner workings of ASI case
            1, and arranging for `0` to be "the offending token" rather than
            `yield`.

    *   Lexical variable names have to be unique within a scope:

        *   Lexical variables (`let` and `const`) can’t be declared more
            than once in a block, or both lexically declared and
            declared with `var`.

        *   Lexically declared variables in a function body can’t have the same
            name as argument bindings.

    *   A lexical variable can’t be named `let`.

    *   Common-sense rules dealing with unicode escape sequences in
        identifiers.

*   Common-sense rules about regular expression literals. (They have to
    actually be valid regular expressions, and unrecognized flags are
    errors.)

*   The number of string parts that a template string can have is
    limited to 2<sup>32</sup> &minus; 1.

*   Invalid Unicode escape sequences, like `\7` or `\09` or `\u{3bjq`, are
    banned in non-tagged templates (in tagged templates, they are allowed).

*   The *SuperCall* syntax is allowed only in derived class
    constructors.

*   `const x;` without an initializer is a Syntax Error.

*   A direct substatement of an `if` statement, loop statement, can’t be a labelled
    `function`.

*   Early errors are used to hook up cover grammars.

    *   Early errors are also used in one case to avoid having to
        specify a very large refinement grammar when *ObjectLiteral*
        almost covers *ObjectAssignmentPattern*:
        [sorry, too complicated to explain](https://tc39.es/ecma262/#sec-object-initializer-static-semantics-early-errors).

*   Early errors are sometimes used to prevent parsers from needing to
    backtrack too much.

    *   When parsing `async ( x = await/a/g )`, you don't know until the
        next token if this is an async arrow or a call to a function named
        `async`. This means you can't even tokenize properly, because in
        the former case the thing following `x =` is two divisions and in
        the latter case it's an *AwaitExpression* of a regular expression.
        So an Early Error forbids having `await` in parameters at all,
        allowing parsers to immediately throw an error if they find
        themselves in this case.

Many strict mode rules are enforced using Early Errors, but others
affect runtime semantics.

<!--
*   I think the rules about assignment targets are related to cover
    grammars. Not sure.

    *   Expressions used in assignment context (i.e. as the operands of `++`
        and `--`, the left operand of assignment, the loop variable of a
        `for-in/for-of` loop, or sub-targets in destructuring) must be valid
        assignment targets.

    *   In destructuring assignment, `[...EXPR] = x` is an error if `EXPR`
        is an array or object destructuring target.

-->


### Boolean parameters (**)

Some nonterminals are parameterized. (Search for “parameterized
production” in [this spec
section](https://tc39.es/ecma262/#sec-grammar-notation).)

Implemented naively (e.g. by macro expansion) in a parser generator,
each parameter could nearly double the size of the parser. Instead, the
parameters must be tracked at run time somehow.


### Lookahead restrictions (**)

*(entangled with: restricted productions)*

TODO (I implemented this by hacking the entire LR algorithm. Most every
part of it is touched, although in ways that seem almost obvious once
you understand LR inside and out.)

(Note: It may seem like all of the lookahead restrictions in the spec
are really just a way of saying “this production takes precedence over
that one”—for example, that the lookahead restriction on
*ExpressionStatement* just means that other productions for statements
and declarations take precedence over it. But that isn't accurate; you
can't have an *ExpressionStatement* that starts with `{`, even if it
doesn't parse as a *Block* or any other kind of statement.)


### Automatic Semicolon Insertion (**)

*(entangled with: restricted productions, slashes)*

Most semicolons at the end of JS statements and declarations “may be
omitted from the source text in certain situations”.  This is called
[Automatic Semicolon
Insertion](https://tc39.es/ecma262/#sec-automatic-semicolon-insertion),
or ASI for short.

The specification for this feature is both very-high-level and weirdly
procedural (“When, as the source text is parsed from left to right, a
token is encountered...”, as if the specification is telling a story
about a browser. As far as I know, this is the only place in the spec
where anything is assumed or implied about the internal implementation
details of parsing.) But it would be hard to specify ASI any other way.

Wrinkles:

1.  Whitespace is significant (including whitespace inside comments).
    Most semicolons in the grammar are optional only at the end of a
    line (or before `}`, or at the end of the program).

2.  The ending semicolon of a `do`-`while` statement is extra optional.
    You can always omit it.

3.  A few semicolons are never optional, like the semicolons in `for (;;)`.

    This means there’s a semicolon in the grammar that is optionally
    optional! This one:

    > *LexicalDeclaration* : *LetOrConst* *BindingList* `;`

    It’s usually optional, but not if this is the *LexicalDeclaration*
    in `for (let i = 0; i < 9; i++)`!

4.  Semicolons are not inserted only as a last resort to avoid
    SyntaxErrors. That turned out to be too error-prone, so there are
    also *restricted productions* (see below), where semicolons are more
    aggressively inferred.

5.  In implementations, ASI interacts with the ambiguity of *slashes*
    (see below).

A recursive descent parser implements ASI by calling a special method
every time it needs to parse a semicolon that might be optional.  The
special method has to peek at the next token and consume it only if it’s
a semicolon. This would not be so bad if it weren’t for slashes.

In a parser generator, ASI can be implemented using an error recovery
mechanism.

I think the [error recovery mechanism in
yacc/Bison](https://www.gnu.org/software/bison/manual/bison.html#Error-Recovery)
is too imprecise—when an error happens, it discards states from the
stack searching for a matching error-handling rule. The manual says
“Error recovery strategies are necessarily guesses.”

But here’s a slightly more careful error recovery mechanism that could
do the job:

1.  For each production in the ES spec grammar where ASI could happen, e.g.

    ```
    ImportDeclaration ::= `import` ModuleSpecifier `;`
                                                    { import_declaration($2); }
    ```

    add an ASI production, like this:

    ```
    ImportDeclaration ::= `import` ModuleSpecifier [ERROR]
                                       { check_asi(); import_declaration($2); }
    ```

    What does this mean? This production can be matched, like any other
    production, but it's a fallback. All other productions take
    precedence.

2.  While generating the parser, treat `[ERROR]` as a terminal
    symbol. It can be included in start sets and follow sets, lookahead,
    and so forth.

3.  At run time, when an error happens, synthesize an `[ERROR]` token.
    Let that bounce through the state machine. It will cause zero or
    more reductions. Then, it might actually match a production that
    contains `[ERROR]`, like the ASI production above.

    Otherwise, we’ll get another error—the entry in the parser table for
    an `[ERROR]` token at this state will be an `error` entry. Then we
    really have a syntax error.

This solves most of the ASI issues:

* [x] Whitespace sensitivity: That's what `check_asi()` is for. It
    should signal an error if we're not at the end of a line.

* [x] Special treatment of `do`-`while` loops: Make an error production,
    but don't `check_asi()`.

* [x] Rule banning ASI in *EmptyStatement* or `for(;;)`:
    Easy, don't create error productions for those.

    * [x] Banning ASI in `for (let x=1 \n x<9; x++)`: Manually adjust
        the grammar, copying *LexicalDeclaration* so that there's a
        *LexicalDeclarationNoASI* production used only by `for`
        statements. Not a big deal, as it turns out.

* [x] Slashes: Perhaps have `check_asi` reset the lexer to rescan the
    next token, if it starts with `/`.

* [ ] Restricted productions: Not solved. Read on.


### Restricted productions (**)

*(entangled with: ASI, slashes)*

Line breaks aren’t allowed in certain places. For example, the following
is not a valid program:

    throw                 // SyntaxError
      new Error();

For another example, this function contains two statements, not one:

    function f(g) {
      return              // ASI
        g();
    }

The indentation is misleading; actually ASI inserts a semicolon at the
end of the first line: `return; g();`. (This function always returns
undefined. The second statement is never reached.)

These restrictions apply even to multiline comments, so the function

```js
function f(g) {
  return /*
    */ g();
}
```
contains two statements, just as the previous example did.

I’m not sure why these rules exist, but it’s probably because (back in
the Netscape days) users complained about the bizarre behavior of
automatic semicolon insertion, and so some special do-what-I-mean hacks
were put in.

This is specified with a weird special thing in the grammar:

> *ReturnStatement* : `return` [no *LineTerminator* here] *Expression* `;`

This is called a *restricted production*, and it’s unfortunately
necessary to go through them one by one, because there are several
kinds. Note that the particular hack required to parse them in a
recursive descent parser is a little bit different each time.

*   After `continue`, `break`, or `return`, a line break triggers ASI.

    The relevant productions are all statements, and in each case
    there’s an alternative production that ends immediately with a
    semicolon: `continue ;` `break ;` and `return ;`.

    Note that the alternative production is *not* restricted: e.g. a
    *LineTerminator* can appear between `return` and `;`:

    ```js
    if (x)
      return        // ok
        ;
    else
      f();
    ```

*   After `throw`, a line break is a SyntaxError.

*   After `yield`, a line break terminates the *YieldExpression*.

    Here the alternative production is simply `yield`, not `yield ;`.

*   In a post-increment or post-decrement expression, there can’t be a
    line break before `++` or `--`.

    The purpose of this rule is subtle. It triggers ASI and thus prevents
    syntax errors:

    ```js
    var x = y       // ok: semicolon inserted here
    ++z;
    ```

    Without the restricted production, `var x = y ++` would parse
    successfully, and the “offending token” would be `z`. It would be
    too late for ASI.

    However, the restriction can of course also *cause* a SyntaxError:

    ```js
    var x = (y
             ++);   // SyntaxError
    ```

As we said, recursive descent parsers can implement these rules with hax.

In a generated parser, there are a few possible ways to implement
them. Here are three. If you are not interested in ridiculous
approaches, you can skip the first two.

*   Treat every token as actually a different token when it appears
    after a line break: `TokenType::LeftParen` and
    `TokenType::LeftParenAfterLineBreak`.  Of course the parser
    generator can treat these exactly the same in normal cases, and
    automatically generate identical table entries (or whatever) except
    in states where there’s a relevant restricted production.

*   Add a special LineTerminator token. Normally, the lexer skips
    newlines and never emits this token. However, if the current state
    has a relevant restricted production, the lexer knows this and emits
    a LineTerminator for the first line break it sees; and the parser
    uses that token to trigger an error or transition to another state,
    as appropriate.

*   When in a state that has a relevant restricted production, change
    states if there’s a line break before the next token.  That is,
    split each such state into two: the one we stay in when there’s not
    a line break, and the one we jump to if there is a line break.

In all cases it’ll be hard to have confidence that the resulting parser
generator is really sound. (That is, it might not properly reject all
ambiguous grammars.) I don’t know exactly what property of the few
special uses in the ES grammar makes them seem benign.


### Slashes (**)

*(entangled with: ASI, restricted productions)*

When you see `/` in a JS program, you don’t know if that’s a
division operator or the start of a regular expression unless you’ve
been paying attention up to that point.

[The spec:](https://tc39.es/ecma262/#sec-ecmascript-language-lexical-grammar)

> There are several situations where the identification of lexical input
> elements is sensitive to the syntactic grammar context that is
> consuming the input elements. This requires multiple goal symbols for
> the lexical grammar.

You might think the lexer could treat `/` as an operator only if the
previous token is one that can be the last token of an expression (a set
that includes literals, identifiers, `this`, `)`, `]`, and `}`).  To see
that this does not work, consider:

```js
{} /x/                  // `/` after `}` is regexp
({} / 2)                // `/` after `}` is division

for (g of /(a)(b)/) {}  // `/` after `of` is regexp
var of = 6; of / 2      // `/` after `of` is division

throw /x/;              // `/` after `throw` is regexp
Math.throw / 2;         // `/` after `throw` is division

++/x/.lastIndex;        // `/` after `++` is regexp
n++ / 2;                // `/` after `++` is division
```

So how can the spec be implemented?

In a recursive descent parser, you have to tell the lexer which goal
symbol to use every time you ask for a token. And you have to make sure,
if you look ahead at a token, but *don’t* consume it, and fall back on
another path that can accept a *RegularExpressionLiteral* or
*DivPunctuator*, that you did not initially lex it incorrectly. We have
assertions for this and it is a bit of a nightmare when we have to touch
it (which is thankfully rare). Part of the problem is that the token
you’re peeking ahead at might not be part of the same production at all.
Thanks to ASI, it might be the start of the next statement, which will
be parsed in a faraway part of the Parser.

A table-driven parser has it easy here! The lexer can consult the state
table and see which kind of token can be accepted in the current
state. This is closer to what the spec actually says.

Two minor things to watch out for:

*   The nonterminal *ClassTail* is used both at the end of
    *ClassExpression*, which may be followed by `/`; and at the end of
    *ClassDeclaration*, which may be followed by a
    *RegularExpressionLiteral* at the start of the next
    statement. Canonical LR creates separate states for these two uses
    of *ClassTail*, but the LALR algorithm will unify them, creating
    some states that have both `/` and *RegularExpressionLiteral* in the
    follow set.  In these states, determining which terminal is actually
    allowed requires looking not only at the current state, but at the
    current stack of states (to see one level of grammatical context).

*   Since this decision depends on the parser state, and automatic
    semicolon insertion adjusts the parser state, a parser may need to
    re-scan a token after ASI.

In other kinds of generated parsers, at least the lexical goal symbol
can be determined automatically.


### Lazy compilation and scoping (**)

*(entangled with: arrow functions)*

JS engines *lazily compile* function bodies. During parsing, when the
engine sees a `function`, it switches to a high-speed parsing mode
(which I will call “Beast Mode”) that just skims the function and checks
for syntax errors. Beast Mode does not compile the code. Beast Mode
doesn’t even create AST nodes. All that will be done later, on demand,
the first time the function is called.

The point is to get through parsing *fast*, so that the script can start
running. In browsers, `<script>` compilation usually must finish before
we can show the user any meaningful content. Any part of it that can be
deferred must be deferred. (Bonus: many functions are never called, so
the work is not just deferred but avoided altogether.)

Later, when a function is called, we can still defer compilation of
nested functions inside it.

So what? Seems easy enough, right? Well...

Local variables in JS are optimized. To generate reasonable code for the
script or function we are compiling, we need to know which of its local
variables are used by nested functions, which we are *not* currently
compiling. That is, we must know which variables *occur free in* each
nested function. So scoping, which otherwise could be done as a separate
phase of compilation, must be done during parsing in Beast Mode.

Getting the scoping of arrow functions right while parsing is tricky,
because it’s often not possible to know when you’re entering a scope
until later. Consider parsing `(a`. This could be the beginning of an
arrow function, or not; we might not know until after we reach the
matching `)`, which could be a long way away.

Annex B.3.3 adds extremely complex rules for scoping for function
declarations which makes this especially difficult. In

```js
function f() {
    let x = 1;
    return function g() {
        {
            function x(){}
        }
        {
            let x;
        }
        return x;
    };
}
```

the function `g` does not use the initial `let x`, but in

```js
function f() {
    let x = 1;
    return function g() {
        {
            {
                function x(){}
            }
            let x;
        }
        return x;
    };
}
```
it does.

[Here](https://dev.to/rkirsling/tales-from-ecma-s-crypt-annex-b-3-3-56go)
is a good writeup of what's going on in these examples.


### Arrow functions, assignment, destructuring, and cover grammars (\*\*\*)

*(entangled with: lazy compilation)*

Suppose the parser is scanning text from start to end, when it sees a
statement that starts with something like this:

```js
let f = (a
```

The parser doesn’t know yet if `(a ...` is *ArrowParameters* or just a
*ParenthesizedExpression*. Either is possible. We’ll know when either
(1) we see something that rules out the other case:

```js
let f = (a + b           // can't be ArrowParameters

let f = (a, b, ...args   // can't be ParenthesizedExpression
```

or (2) we reach the matching `)` and see if the next token is `=>`.

Probably (1) is a pain to implement.

To keep the language nominally LR(1), the standard specifies a “cover
grammar”, a nonterminal named
*CoverParenthesizedExpressionAndArrowParameterList* that is a superset
of both *ArrowParameters* and *ParenthesizedExpression*.  The spec
grammar uses the *Cover* nonterminal in both places, so technically `let
f = (a + b) => 7;` and `let f = (a, b, ...args);` are both syntactically
valid *Script*s, according to the formal grammar. But there are Early
Error rules (that is, plain English text, not part of the formal
grammar) that say arrow functions’ parameters must match
*ArrowParameters*, and parenthesized expressions must match
*ParenthesizedExpression*.

So after the initial parse, the implementation must somehow check that
the *CoverParenthesizedExpressionAndArrowParameterList* really is valid
in context. This complicates lazy compilation because in Beast Mode we
are not even building an AST. It’s not easy to go back and check.

Something similar happens in a few other cases: the spec is written as
though syntactic rules are applied after the fact:

*   *CoverCallExpressionAndAsyncArrowHead* covers the syntax `async(x)`
    which looks like a function call and also looks like the beginning
    of an async arrow function.

*   *ObjectLiteral* is a cover grammar; it covers both actual object
    literals and the syntax `propertyName = expr`, which is not valid in
    object literals but allowed in destructuring assignment:

    ```js
    var obj = {x = 1};  // SyntaxError, by an early error rule

    ({x = 1} = {});   // ok, assigns 1 to x
    ```

*   *LeftHandSideExpression* is used to the left of `=` in assignment,
    and as the operand of postfix `++` and `--`. But this is way too lax;
    most expressions shouldn’t be assigned to:

    ```js
    1 = 0;  // matches the formal grammar, SyntaxError by an early error rule

    null++;   // same

    ["a", "b", "c"]++;  // same
    ```
