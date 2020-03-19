## jsparagus/js_parser: Generating a parser for JavaScript

In this directory:

*   **esgrammar.pgen** A grammar for the mini-language the ECMAScript
    standard uses to describe ES grammar.

*   **es.esgrammar** - The actual grammar for ECMAScript, in emu-grammar
    format, extracted automatically from the spec.

*   **extract_es_grammar.py** - The script that creates *es.esgrammar*.

*   **es-simplified.esgrammar** - A hacked version of *es.esgrammar* that
    jsparagus can actually handle.

*   **generate_js_parser_tables.py** - A script to generate a JS parser
    based on *es-simplified.esgrammar*.  Read on for instructions.


## How to run it

To generate a parser, follow these steps:

```console
$ cd ..
$ make init
$ make all
```

**Note:** The last step currently takes about 35 seconds to run on my
laptop.  jsparagus is slow.

Once you're done, to see your parser run, try this:

```console
$ cd crates/driver
$ cargo run --release
```

The build also produces a copy of the JS parser in Python.
After `make all`, you can use `make jsdemo` to run that.


### How simplified is "es-simplified"?

Here are the differences between *es.esgrammar*, the actual ES grammar,
and *es-simplified.esgrammar*, the simplified version that jsparagus can
actually handle:

*   The four productions with [~Yield] and [~Await] conditions are dropped.
    This means that `yield` and `await` do not match *IdentifierReference*
    or *LabelIdentifier*. I think it's better to do that in the lexer.

*   Truncated lookahead.

    `ValueError: unsupported: lookahead > 1 token, [['{'], ['function'], ['async', ('no-LineTerminator-here',), 'function'], ['class'], ['let', '[']]`

*   Delete a rule that uses `but not` since it's not implemented.

        Identifier :
          IdentifierName but not ReservedWord

    Making sense of this rule in the context of an LR parser is an
    interesting task; see issue #28.

*   Ban loops of the form `for (async of EXPR) STMT` by adjusting a
    lookahead assertion. The grammar is not LR(1).
