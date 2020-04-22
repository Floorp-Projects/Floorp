"""Generate a simple LR0 state graph from a CanonicalGrammar.

The resulting graph may contain inconsistent states, which must be resolved by the
ParseTable before a parser can be generated.
"""

from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

import collections
from dataclasses import dataclass
import typing

from .actions import (Action, CheckNotOnNewLine, FunCall, Lookahead,
                      OutputExpr, Reduce, Seq)
from .ordered import OrderedFrozenSet
from .grammar import (CallMethod, Element, End, ErrorSymbol, Grammar,
                      LookaheadRule, NoLineTerminatorHere, Nt, ReduceExpr,
                      ReduceExprOrAccept, Some)
from .rewrites import CanonicalGrammar, Prod
from . import types


# ## LR parsers: Why?
#
# Consider a single production `expr ::= expr "+" term` being parsed in a
# recursive descent parser. As we read the source left to right, our parser's
# internal state looks like this (marking our place with a dot):
#
#     expr ::= · expr "+" term
#     expr ::= expr · "+" term
#     expr ::= expr "+" · term
#     expr ::= expr "+" term ·
#
# As we go, we build an AST. First we parse an *expr* and temporarily set it
# aside. Then we expect to see a `+` operator. Then we parse a *term*. Then,
# having got to the end, we create an AST node for the whole addition
# expression.
#
# Since the grammar is nested, at run time we really have a stack of these
# intermediate states.
#
# But how do we decide which production we should be matching? Often the first
# token just tells us: the `while` keyword means there's a `while` statement
# coming up. Grammars in which this is always the case are called LL(1). But
# while it's possible to wrangle *most* of the ES grammar into an LL(1) form,
# not everything works out. For example, here's the ES assignment syntax (much
# simplified):
#
#     assignment ::= sum
#     assignment ::= primitive "=" assignment
#     sum ::= primitive
#     sum ::= sum "+" primitive
#     primitive ::= VAR
#
# Note that the bogus assignment `a + b = c` doesn't parse because `a + b`
# isn't a primitive.
#
# Suppose we want to parse an expression, and the first token is `a`. We don't
# know yet which *assignment* production to use. So this grammar is not in
# LL(1).
#
#
# ## LR parsers: How
#
# An LR parser generator allows for a *superposition* of states. While parsing,
# we can sometimes have multiple productions at once that might match. It's
# like how in quantum theory, Schrödinger’s cat can tentatively be both alive
# and dead until decisive information is observed.
#
# As we read `a = b + c`, our parser's internal state is like this
# (eliding a few steps, like how we recognize that `a` is a primitive):
#
#     current point in input  superposed parser state
#     ----------------------  -----------------------
#     · a = b + c             assignment ::= · sum
#                             assignment ::= · primitive "=" assignment
#
#       (Then, after recognizing that `a` is a *primitive*...)
#
#     a · = b + c             sum ::= primitive ·
#                             assignment ::= primitive · "=" assignment
#
#       (The next token, `=`, rules out the first alternative,
#       collapsing the waveform...)
#
#     a = · b + c             assignment ::= primitive "=" · assignment
#
#       (After recognizing that `b` is a primitive, we again have options:)
#
#     a = b · + c             sum ::= primitive ·
#                             assignment ::= primitive · "=" assignment
#
# And so on. We call each dotted production an "LR item", and the superposition
# of several LR items is called a "state".  (It is not meant to be clear yet
# just *how* the parser knows which rules might match.)
#
# Since the grammar is nested, at run time we'll have a stack of these parser
# state superpositions.
#
# The uncertainty in LR parsing means that code for an LR parser written by
# hand, in the style of recursive descent, would read like gibberish. What we
# can do instead is generate a parser table.


@dataclass(frozen=True, order=True)
class LRItem:
    """A snapshot of progress through a single specific production.

    *   `prod_index` identifies the production. (Every production in the grammar
        gets a unique index; see the loop that computes
        prods_with_indexes_by_nt.)

    *   `offset` is the position of the cursor within the production.

    `lookahead` and `followed_by` are two totally different kinds of lookahead.

    *   `lookahead` is the LookaheadRule, if any, that applies to the immediately
        upcoming input. It is present only if this LRItem is subject to a
        `[lookahead]` restriction; otherwise it's None. These restrictions can't
        extend beyond the end of a production, or else the grammar is invalid.
        This implements the lookahead restrictions in the ECMAScript grammar.
        It is not part of any account of LR I've seen.

    *   `followed_by` is a completely different kind of lookahead restriction.
        This is the kind of lookahead that is a central part of canonical LR
        table generation.  It applies to the token *after* the whole current
        production, so `followed_by` always applies to completely different and
        later tokens than `lookahead`.  `followed_by` is a set of terminals; if
        `None` is in this set, it means `END`, not that the LRItem is
        unrestricted.
    """

    prod_index: int
    offset: int
    lookahead: typing.Optional[LookaheadRule]
    followed_by: OrderedFrozenSet[typing.Optional[str]]


# A Term is the label on an edge from one state to another. It's normally a
# terminal, nonterminal, or epsilon action. A state can also have a special
# catchall edge, labeled with an ErrorSymbol.
ShiftedTerm = typing.Union[str, Nt, ErrorSymbol]
Term = typing.Union[ShiftedTerm, Action]


def on_stack(grammar: Grammar, term: Element) -> bool:
    """Returns whether an element of a production is consuming stack space or
    not."""
    if isinstance(term, Nt):
        return True
    elif grammar.is_terminal(term):
        return True
    elif isinstance(term, LookaheadRule):
        return False
    elif isinstance(term, ErrorSymbol):
        return True
    elif isinstance(term, End):
        return True
    elif term is NoLineTerminatorHere:
        # No line terminator is a property of the next token being shifted. It
        # is implemented as an action which once shifted past the next term,
        # will check whether the previous term shifted is on a new line.
        return False
    elif isinstance(term, CallMethod):
        return False
    raise ValueError(term)


def callmethods_to_funcalls(
        expr: ReduceExprOrAccept,
        pop: int,
        ret: str,
        depth: int,
        funcalls: typing.List[FunCall]
) -> OutputExpr:
    """Lower a reduce-expression to the OutputExpr language.

    CallMethod expressions are replaced with FunCalls; all new FunCalls created
    in this way are appended to `funcalls`.
    """

    # TODO: find a way to carry alias sets to here.
    alias_set = ["parser"]
    if isinstance(expr, int):
        stack_index = pop - expr
        if depth == 0:
            call = FunCall("id", (stack_index,), fallible=False,
                           trait=types.Type("AstBuilder"), set_to=ret,
                           alias_read=alias_set, alias_write=alias_set)
            funcalls.append(call)
            return ret
        else:
            return stack_index
    elif isinstance(expr, Some):
        res = callmethods_to_funcalls(expr.inner, pop, ret, depth, funcalls)
        # "type: ignore" because Some is not generic, unfortunately.
        return Some(res)  # type: ignore
    elif expr is None:
        return None
    elif isinstance(expr, CallMethod):
        def convert_args(args: typing.Iterable[ReduceExpr]) -> typing.Iterator[OutputExpr]:
            for i, arg in enumerate(args):
                yield callmethods_to_funcalls(arg, pop, ret + "_{}".format(i), depth + 1, funcalls)
        args = tuple(convert_args(expr.args))
        call = FunCall(expr.method, args,
                       trait=expr.trait,
                       fallible=expr.fallible,
                       set_to=ret,
                       alias_read=alias_set,
                       alias_write=alias_set)
        funcalls.append(call)
        return ret
    elif expr == "accept":
        call = FunCall("accept", (),
                       trait=types.Type("ParserTrait"),
                       fallible=False,
                       set_to=ret,
                       alias_read=alias_set,
                       alias_write=alias_set)
        funcalls.append(call)
        return ret
    else:
        raise ValueError(expr)


class LR0Generator:
    """Provide a way to iterate over the grammar, given a set of LR items."""
    __slots__ = [
        "grammar",
        "lr_items",
        "key",
        "_hash",
    ]

    grammar: CanonicalGrammar
    lr_items: OrderedFrozenSet[LRItem]
    key: str
    _hash: int

    def __init__(
            self,
            grammar: CanonicalGrammar,
            lr_items: typing.Iterable[LRItem] = ()
    ) -> None:
        self.grammar = grammar
        self.lr_items = OrderedFrozenSet(lr_items)
        # This is used to reuse states which have already been encoded.
        self.key = "".join(repr((item.prod_index, item.offset)) + "\n"
                           for item in sorted(self.lr_items))
        self._hash = hash(self.key)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, LR0Generator) and self.key == other.key

    def __hash__(self) -> int:
        return self._hash

    def __str__(self) -> str:
        s = ""
        for lr_item in self.lr_items:
            s += self.grammar.grammar.lr_item_to_str(self.grammar.prods, lr_item)
            s += "\n"
        return s

    def stable_locations(self) -> OrderedFrozenSet[str]:
        locations = []
        for lr_item in self.lr_items:
            locations.append(self.grammar.grammar.lr_item_to_str(self.grammar.prods, lr_item))
        return OrderedFrozenSet(sorted(locations))

    @staticmethod
    def start(grammar: CanonicalGrammar, nt: Nt) -> LR0Generator:
        lr_items: typing.List[LRItem] = []
        # Visit the initial non-terminal, as well as all the non-terminals
        # which are at the left of each productions.
        todo: typing.Deque[Nt] = collections.deque()
        visited_nts = []
        todo.append(nt)
        while todo:
            nt = todo.popleft()
            if nt in visited_nts:
                continue
            visited_nts.append(nt)
            for prod_index, _ in grammar.prods_with_indexes_by_nt[nt]:
                assert isinstance(prod_index, int)
                lr_items.append(LRItem(
                    prod_index=prod_index,
                    offset=0,
                    lookahead=None,
                    followed_by=OrderedFrozenSet(),
                ))

                prod = grammar.prods[prod_index]
                assert isinstance(prod, Prod)
                try:
                    term = prod.rhs[0]
                    if isinstance(term, Nt):
                        todo.append(term)
                except IndexError:
                    pass
        return LR0Generator(grammar, lr_items)

    def transitions(self) -> typing.Dict[Term, LR0Generator]:
        """Returns the dictionary which maps the state transitions with the next
        LR0Generators. This can be used to generate the states and the
        transitions between the states of an LR0 parse table."""
        followed_by: typing.DefaultDict[Term, typing.List[LRItem]]
        followed_by = collections.defaultdict(list)
        for lr_item in self.lr_items:
            self.item_transitions(lr_item, followed_by)

        return {k: LR0Generator(self.grammar, lr_items)
                for k, lr_items in followed_by.items()}

    def item_transitions(
            self,
            lr_item: LRItem,
            followed_by: typing.DefaultDict[Term, typing.List[LRItem]]
    ) -> None:
        """Given one LRItem, register all the transitions and LR Items reachable
        through these transitions."""
        prod = self.grammar.prods[lr_item.prod_index]
        assert isinstance(prod, Prod)

        # Read the term located at the offset in the production.
        if lr_item.offset < len(prod.rhs):
            term = prod.rhs[lr_item.offset]
            if isinstance(term, Nt):
                pass
            elif self.grammar.grammar.is_terminal(term):
                pass
            elif isinstance(term, LookaheadRule):
                term = Lookahead(term.set, term.positive)
            elif isinstance(term, ErrorSymbol):
                # ErrorSymbol as considered as terminals. These terminals would
                # be produced by the error handling code which produces these
                # error symbols on-demand.
                pass
            elif isinstance(term, End):
                # End is considered as a terminal which is produduced once by
                # the lexer upon reaching the end. However, the parser might
                # finish without consuming the End terminal, if there is no
                # ambiguity on whether the End is expected.
                pass
            elif term is NoLineTerminatorHere:
                # Check whether the following terminal is on a new line. If
                # not, this would produce a syntax error. The argument is the
                # terminal offset.
                term = CheckNotOnNewLine()
            elif isinstance(term, CallMethod):
                funcalls: typing.List[FunCall] = []
                pop = sum(1 for e in prod.rhs[:lr_item.offset] if on_stack(self.grammar.grammar, e))
                callmethods_to_funcalls(term, pop, "expr", 0, funcalls)
                term = Seq(funcalls)

        elif lr_item.offset == len(prod.rhs):
            # Add the reduce operation as a state transition in the generated
            # parse table. (TODO: this supposed that the canonical form did not
            # move the reduce action to be part of the production)
            pop = sum(1 for e in prod.rhs if on_stack(self.grammar.grammar, e))
            term = Reduce(prod.nt, pop)
            expr = prod.reducer
            if expr is not None:
                funcalls = []
                callmethods_to_funcalls(expr, pop, "value", 0, funcalls)
                term = Seq(funcalls + [term])
        else:
            # No edges after the reduce operation.
            return

        # Add terminals, non-terminals and lookahead actions, as transitions to
        # the next LR Item.
        new_transition = term not in followed_by
        followed_by[term].append(LRItem(
            prod_index=lr_item.prod_index,
            offset=lr_item.offset + 1,
            lookahead=None,
            followed_by=OrderedFrozenSet(),
        ))

        # If the term is a non-terminal, then recursively add transitions from
        # the beginning of all the productions which are matching this
        # non-terminal.
        #
        # Only do it once per non-terminal to avoid infinite recursion on
        # left-recursive grammars.
        if isinstance(term, Nt) and new_transition:
            for prod_index, _ in self.grammar.prods_with_indexes_by_nt[term]:
                assert isinstance(prod_index, int)
                self.item_transitions(LRItem(
                    prod_index=prod_index,
                    offset=0,
                    lookahead=None,
                    followed_by=OrderedFrozenSet(),
                ), followed_by)
