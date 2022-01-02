## What I learned, what I wonder


### Stab 5 (simple LR, LR(1), then LALR(1))

Well. I learned enough to implement this, although there is still much I
don't understand.

I learned a bit about what kind of phenomenon can render a grammar
outside XLL(1) (that is, LL(1) as extended by automated left-factoring
and left-recursion elimination); see `testFirstFirstConflict` in
`test.py` for a contrived example, and `testLeftHandSideExpression` for
a realistic one.

I learned that the shift-reduce operator precedence parser I wrote for
SpiderMonkey is even less like a typical LR parser than I imagined.

I was stunned to find that the SLR parser I wrote first, including the
table generator, was *less* code than the predictive LL parser of stab
4. However, full LR(1) took rather a lot of code.

I learned that I will apparently hand-code the computation of transitive
closures of sets under relations ten times before even considering
writing a general algorithm. The patterns I have written over and over
are: 1. `while not done:` visit every element already in the set,
iterating to a fixed point, which is this ludicrous O(*n*<sup>2</sup>)
in the number of pairs in the relation; 2. depth-first graph walking
with cycle detection, which can overflow the stack.

I learned three ways to hack features into an LR parser generator (cf. how
easy it is to hack stuff into a recursive descent parser). The tricks I
know are:

1.  Add custom items. To add lookahead assertions, I just added a
    lookahead element to the LRItem tuple. The trick then is to make
    sure you are normalizing states that are actually identical, to
    avoid combinatorial explosion—and eventually, I expect, table
    compression.

2.  Add custom actions. I think I can support automatic semicolon
    insertion by replacing the usual error action of some states with a
    special ASI actions.

3.  Desugaring. The
    [ECMAScript standard](https://tc39.es/ecma262/#sec-grammar-notation)
    describes optional elements and parameterized nonterminals this way,
    and for now at least, that's how we actually implement them.

There's a lot still to learn here.

*   OMG, what does it all mean? I'm getting more comfortable with the
    control flow ("calls" and "returns") of this system, but I wouldn't
    say I understand it!

*   Why is lookahead, past the end of the current half-parsed
    production, part of an LR item? What other kinds of item
    embellishment could be done instead?

*   In what sense is an LR parser a DFA? I implemented it, but there's
    more to it that I haven't grokked yet.

*   Is there just one DFA or many? What exactly is the "derived" grammar
    that the DFA parses? How on earth does it magically turn out to be
    regular?  (This depends on it not extending past the first handle,
    but I still don't quite see.)

*   If I faithfully implement the algorithms in the book, will it be
    less of a dumpster fire? Smaller, more factored?

*   How can I tell if a transformation on grammars preserves the
    property of being LR(k)? Factoring out a nonterminal, for example,
    may not preserve LR(k)ness. Inlining probably always does.

*   Is there some variant of this that treats nonterminals more like
    terminals? It's easy to imagine computing start sets and follow sets
    that contain both kinds of symbols. Does that buy us anything?


Things I noticed:

*   I think Yacc allows bits of code in the middle of productions:

        nt1: T1 T2 nt2 { code1(); } T3 nt3 T4 { code2(); }

    That could be implemented by introducing a synthetic production
    that contains everything up to the first code block:

        nt1_aux: T1 T2 nt2 { code1(); }
        nt1: nt1_aux T3 nt3 T4 { code2(); }

    There is a principle that says code should happen only at the end of
    a production: because LR states are superpositions of items. We
    don't know which production we are really parsing until we reduce,
    so we don't know which code to execute.

*   Each state is reachable from an initial state by a finite sequence
    of "pushes", each of which pushes either a terminal (a shift action)
    or a nonterminal (a summary of a bunch of parsing actions, ending
    with a reduce).

    States can sometimes be reached multiple ways (it's a state
    transition graph). But regardless of which path you take, the symbols
    pushed by the last few steps always match the symbols appearing to
    the left of point in each of the state's LR items. (This implies
    that those items have to agree on what has happened. Might make a
    nice assertion.)



### Stab 4 (nonrecursive table-driven predictive LL parser)

I learned that testing that a Python program can do something deeply
recursive is kind of nontrivial. :-\

I learned that the predictive parser still takes two stacks (one
representing the future and one representing the past). It's not magic!
This makes me want to hop back to stab 3, optimize away the operand
stack, and see what kind of code I can get.

It seems like recursive descent would be faster, but the table-driven
parser could be made to support incremental parsing (the state of the
algorithm is "just data", a pair of stacks, neither of which is the
parser program's native call stack).


### Stab 3 (recursive descent with principled left-recursion-elimination and left-factoring)

I learned how to eliminate left recursion in a grammar (Algorithm 4.1
from the book). I learned how to check that a grammar is LL(1) using
the start and follow sets, although I didn't really learn what LL(1)
means in any depth. (I'm just using it as a means to prove that the
grammar is unambiguous.)

I learned from the book how to do a table-driven "nonrecursive
predictive parser". Something to try later.

I came up with the "reduction symbol" thing. It seems to work as
expected!  This allows me to transform the grammar, but still generate
parse trees reflecting the source grammar. However, the resulting code
is inefficient. Further optimization would improve it, but the
predictive parser will fare better even without optimization.

I wonder what differences there are between LL(1) and LR(1) grammars.
(The book repeatedly says they are different, but the distinctions it
draws suggest differences like: left-recursive grammars can be LR but
never LL.  That particular difference doesn't matter much to me, because
there's an algorithm for eliminating left recursion.)


### Stab 2 (recursive descent with ad hoc immediate-left-recursion-elimination)

I learned it's easy for code to race ahead of understanding.
I learned that a little feature can mean a lot of complexity.

I learned that it's probably hard to support indirect left-recursion using this approach.
We're able to twist left-recursion into a `while` loop because what we're doing is local to a single nonterminal's productions,
and they're all parsed by a single function.
Making this work across function boundaries would be annoying,
even ignoring the possibility that a nonterminal can be involved in multiple left-call cycles.

I wonder if the JS spec uses any indirect left-recursion.

I wonder if there's a nice formalization of a "grammar with actions" that abstracts away "implementation details",
so that we could prove two grammars equivalent,
not just in that they describe the same language,
but equivalent in output.
This could help me explore "grammar rewrites",
which could lead to usable optimizations.

I noticed that the ES spec contains this:

> ### 13.6 The if Statement
> #### Syntax
> ```
> IfStatement[Yield, Await, Return]:
>     if ( Expression[+In, ?Yield, ?Await] ) Statement[?Yield, ?Await, ?Return] else Statement[?Yield, ?Await, ?Return]
>     if ( Expression[+In, ?Yield, ?Await] ) Statement[?Yield, ?Await, ?Return]
> ```
>
> Each `else` for which the choice of associated `if` is ambiguous shall
> be associated with the nearest possible `if` that would otherwise have
> no corresponding `else`.

I wonder if this prose is effectively the same as adding a negative lookahead assertion
"[lookahead &ne; `else`]" at the end of the shorter production.

(I asked bterlson and he thinks so.)

I wonder if follow sets can be usefully considered as context-dependent.
What do I mean by this?
For example, `function` is certainly in the follow set of *Statement* in JS,
but there are plenty of contexts, like the rule `do Statement while ( Expression ) ;`,
where the nested *Statement* is never followed by `function`.
But does it matter?
I think it only matters if you're interested in better error messages.
Follow sets only matter to detect ambiguity in a grammar,
and *Statement* is ambiguous if it's ambiguous in *any* context.


### Stab 1 (very naive recursive descent)

I learned that if you simply define a grammar as a set of rules,
there are all sorts of anomalies that can come up:

*   Vacant nonterminals (that do not match any input strings);

*   Nonterminals that match only infinite strings, like `a ::= X a`.

*   Cycles ("busy loops"), like `a ::= a`.
    These always introduce ambiguity.
    (You can also have cycles through multiple nonterminals:
    `a ::= b; b ::= a`.)

These in particular are easy to test for, with no false positives.
I wonder if there are other anomalies,
and if the "easiness" generalizes to all of them, and why.

I know what it means for a grammar to be *ambiguous*:
it means there's at least one input with multiple valid parses.
I understand that parser generators can check for ambiguity.
But it's easiest to do so by imposing draconian restrictions.
I learned the "dangling `else` problem" is an ambiguity in exactly this sense.
I wonder if there's a principled way to deal with it.

I know that a parse is a constructive proof that a string matches a grammar.

I learned that start sets are important even in minimal parser generators.
This is interesting because they'll be a bit more interesting to compute
once we start considering empty productions.
I wonder if it turns out to still be pretty easy.
Does the start set of a possibly-empty production include its follow set?
(According to the dragon book, you add epsilon to the start set in this case.)


### Nice grammars

I learned that the definition of a "grammar"
as a formal description of a language (= a set of strings)
is incomplete.

Consider the Lisp syntax we're using:

```
sexpr ::= SYMBOL
sexpr ::= "(" tail

tail ::= ")"
tail ::= sexpr tail
```

Nobody wants to parse Lisp like that.
There are two problems.

One is expressive.
The `"("` and `")"` tokens should appear in the same production.
That way, the grammar says declaratively: these marks always appear in properly nesting pairs.

```
sexpr ::= SYMBOL
sexpr ::= "(" list ")"

list ::= [empty]
list ::= sexpr list
```

The other problem has to do with *what you've got* when you get an automatically generated parse.
A grammar is more than just a description of a language,
to the extent we care about the form of the parse trees we get out of the parser.

A grammar is a particular way of writing a parser,
and since we care about the parser's output,
we care about details of the grammar that would be mere "implementation details" otherwise.
