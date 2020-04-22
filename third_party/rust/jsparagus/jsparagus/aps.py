from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

import typing
from dataclasses import dataclass
from .grammar import Nt
from .lr0 import ShiftedTerm, Term
from .actions import Action, Reduce


StateId = int

# Hack to avoid circular reference between this module and parse_table.py
StateAndTransition = typing.Any
ParseTable = typing.Any


def shifted_path_to(pt: ParseTable, n: int, right_of: Path) -> typing.Iterator[Path]:
    "Compute all paths with n shifted terms, ending with right_of."
    assert isinstance(right_of, list) and len(right_of) >= 1
    if n == 0:
        yield right_of
    state = right_of[0].src
    assert isinstance(state, int)
    for edge in pt.states[state].backedges:
        if not pt.is_term_shifted(edge.term):
            print(repr(edge))
            print(pt.states[edge.src])
        assert pt.is_term_shifted(edge.term)
        if pt.term_is_stacked(edge.term):
            s_n = n - 1
            if n == 0:
                continue
        else:
            s_n = n
        from_edge = Edge(edge.src, edge.term)
        for path in shifted_path_to(pt, s_n, [from_edge] + right_of):
            yield path


def reduce_path(pt: ParseTable, shifted: Path) -> typing.Iterator[Path]:
    """Compute all paths which might be reduced by a given action. This function
    assumes that the state is reachable from the starting goals, and that
    the depth which is being queried has valid answers."""
    assert len(shifted) >= 1
    action = shifted[-1].term
    assert isinstance(action, Action)
    assert action.update_stack()
    reducer = action.reduce_with()
    assert isinstance(reducer, Reduce)
    nt = reducer.nt
    depth = reducer.pop + reducer.replay
    if depth > 0:
        # We are reducing at least one element from the stack.
        stacked = [i for i, e in enumerate(shifted) if pt.term_is_stacked(e.term)]
        if len(stacked) < depth:
            # We have not shifted enough elements to cover the full reduce
            # rule, start looking for context using backedges.
            shifted_from = 0
            depth -= len(stacked)
        else:
            # We shifted much more elements than necessary for reducing,
            # just start from the first stacked element which correspond to
            # consuming all stack element reduced.
            shifted_from = stacked[-depth]
            depth = 0
        shifted_end = shifted[shifted_from:]
    else:
        # We are reducing no element from the stack.
        shifted_end = shifted[-1:]
    for path in shifted_path_to(pt, depth, shifted_end):
        # NOTE: When reducing, we might be tempted to verify that the
        # reduced non-terminal is part of the state we are reducing to, and
        # it surely is for one of the shifted path. However, this would be
        # an error in an inconsistent grammar. (see issue #464)
        #
        # Thus, we might yield plenty of path which are not reducing the
        # expected non-terminal, but these are expected to be filtered out
        # by the APS, as the inability of shifting these non-terminals
        # would remove these cases.
        assert pt.assume_inconsistent or nt in pt.states[path[0].src].nonterminals
        yield path


@dataclass(frozen=True)
class Edge:
    """An edge in a Parse table is a tuple of a source state and the term followed
    to exit this state. The destination is not saved here as it can easily be
    inferred by looking it up in the parse table.

    Note, the term might be `None` if no term is specified yet. This is useful
    for specifying the last state in a Path.

      src: Index of the state from which this directed edge is coming from.

      term: Edge transition value, this can be a terminal, non-terminal or an
          action to be executed on an epsilon transition.
    """
    src: StateId
    term: typing.Optional[Term]

    def stable_str(self, states: typing.List[StateAndTransition]) -> str:
        return "{} -- {} -->".format(states[self.src].stable_hash, str(self.term))

    def __str__(self) -> str:
        return "{} -- {} -->".format(self.src, str(self.term))


# A path through the state graph.
#
# `e.src for e in path` is the sequence of states visited, and
# `e.term for e in path[:-1]` is the sequence of edges traversed.
# `path[-1].term` should be ignored and is often None.
Path = typing.List[Edge]


@dataclass(frozen=True)
class APS:
    # To fix inconsistencies of the grammar, we have to traverse the grammar
    # both forward by using the lookahead and backward by using the state
    # recovered from following unwind actions.
    #
    # To do so we define the notion of abstract parser state (APS), which is a
    # class which represents the known state of the parser, relative to its
    # starting point.
    #
    # An APS does not exclusively start at the parser entry point, but starts
    # from any state of the parse table by calling `APS.start`. Then we walk
    # the parse table forward, as-if we were shifting tokens or epsilon edges
    # in the parse table. The function `aps.shift_next(parse_table)` will
    # explore all possible futures reachable from the starting point.
    #
    # As the parse table is explored, new APS are produced by
    # `aps.shift_next(parse_table)`, which are containing the new state of the
    # parser and the history which has been seen by the APS since it started.
    __slots__ = ['stack', 'shift', 'lookahead', 'replay', 'history', 'reducing']

    # This is the known stack at the location where we started investigating.
    # As more history is discovered by resolving unwind actions, this stack
    # would be filled with the predecessors which have been visited before
    # reaching the starting state.
    stack: Path

    # This is the stack as manipulated by an LR parser. States are shifted to
    # it, including actions, and popped from it when visiting a unwind action.
    shift: Path

    # This is the list of terminals and non-terminals encountered by shifting
    # edges which are not replying tokens.
    lookahead: typing.List[ShiftedTerm]

    # This is the list of lookahead terminals and non-terminals which remains
    # to be shifted. This list corresponds to terminals and non-terminals which
    # were necessary for removing inconsistencies, but have to be replayed
    # after shifting the reduced non-terminals.
    replay: typing.List[typing.Union[str, Nt]]

    # This is the list of edges visited since the starting state.
    history: Path

    # This is a flag which is used to distinguish whether the next term to be
    # replayed is the result of a Reduce action or not. When reducing, epsilon
    # transitions should be ignored. This flag is useful to implement Unwind
    # and Reduce as 2 different actions.
    reducing: bool

    @staticmethod
    def start(state: StateId) -> APS:
        "Return an Abstract Parser State starting at a given state of a parse table"
        edge = Edge(state, None)
        return APS([edge], [edge], [], [], [], False)

    def shift_next(self, pt: ParseTable) -> typing.Iterator[APS]:
        """Yield an APS for each state reachable from this APS in a single step,
        by handling a single term (terminal, nonterminal, or action).

        All yielded APS are representing context information around the same
        starting state as `self`, either by having additional lookahead terms,
        or a larger stack representing the path taken to reach the starting
        state.

        For each outgoing edge, it builds a new APS which represents the state
        of the Parser if we were to have taken this edge. Only valid APS are
        yielded given the context provided by `self`.

        For example, we cannot reduce to a path which is different than what is
        already present in the `shift` list, or shift a term different than the
        next term to be shifted from the `replay` list.
        """

        # The actual type of parameter `pt` is ParseTable, but this would
        # require a cyclic dependency, so we bail out of the type system using
        # typing.Any.

        st, sh, la, rp, hs = self.stack, self.shift, self.lookahead, self.replay, self.history
        last_edge = sh[-1]
        state = pt.states[last_edge.src]
        if self.replay == []:
            for term, to in state.shifted_edges():
                edge = Edge(last_edge.src, term)
                new_sh = self.shift[:-1] + [edge]
                to = Edge(to, None)
                yield APS(st, new_sh + [to], la + [term], rp, hs + [edge], False)
        else:
            term = self.replay[0]
            rp = self.replay[1:]
            if term in state:
                edge = Edge(last_edge.src, term)
                new_sh = self.shift[:-1] + [edge]
                to = state[term]
                to = Edge(to, None)
                yield APS(st, new_sh + [to], la, rp, hs + [edge], False)

        term = None
        rp = self.replay
        for a, to in state.epsilon:
            edge = Edge(last_edge.src, a)
            prev_sh = self.shift[:-1] + [edge]
            # TODO: Add support for Lookahead and flag manipulation rules, as
            # both of these would invalide potential reduce paths.
            if a.update_stack():
                if self.reducing:
                    # When reducing, do not attempt to execute any actions
                    # which might update the stack. Without this restriction,
                    # we might loop on Optional rules. Which would not match
                    # the expected behaviour of the parser.
                    continue
                reducer = a.reduce_with()
                for path in reduce_path(pt, prev_sh):
                    # The reduced path contains the chains of state shifted,
                    # including epsilon transitions, such that it consume as
                    # many shifted terms as is expected to be replayed and
                    # popped by the Reduce action. However, it is not guarantee
                    # that the path will come back to an edge which can consume
                    # the non-terminal.

                    # print(
                    #     "Compare shifted path, with reduced path:\n"
                    #     "\tshifted = {},\n"
                    #     "\treduced = {},\n"
                    #     "\taction = {},\n"
                    #     "\tnew_path = {}\n"
                    #     .format(
                    #         " ".join(edge_str(e) for e in prev_sh),
                    #         " ".join(edge_str(e) for e in path),
                    #         str(a),
                    #         " ".join(edge_str(e) for e in reduced_path)))
                    if prev_sh[-len(path):] != path[-len(prev_sh):]:
                        # If the reduced production does not match the shifted
                        # state, then this reduction does not apply. This is
                        # the equivalent result as splitting the parse table
                        # based on the predecessor.
                        continue

                    # The stack corresponds to the stack present at the
                    # starting point. The shift list correspond to the actual
                    # parser stack as we iterate through the state machine.
                    # Each time we consume all the shift list, this implies
                    # that we had extra stack elements which were not present
                    # initially, and therefore we are learning about the
                    # context.
                    new_st = path[:max(len(path) - len(prev_sh), 0)] + st
                    assert pt.is_valid_path(new_st)

                    # The shift list corresponds to the stack which is used in
                    # an LR parser, in addition to all the states which are
                    # epsilon transitions. We pop from this list the reduced
                    # path, as long as it matches. Then all popped elements are
                    # replaced by the state that we visit after replaying the
                    # non-terminal reduced by this action.
                    new_sh = prev_sh[:-len(path)] + [Edge(path[0].src, None)]
                    assert pt.is_valid_path(new_sh)

                    # When reducing, we replay terms which got previously
                    # pushed on the stack as our lookahead. These terms are
                    # computed here such that we can traverse the graph from
                    # `to` state, using the replayed terms.
                    replay = reducer.replay
                    new_rp = [reducer.nt]
                    if replay > 0:
                        stacked_terms = [edge.term for edge in path if pt.term_is_stacked(edge.term)]
                        new_rp = new_rp + stacked_terms[-replay:]
                    new_rp = new_rp + rp
                    new_la = la[:max(len(la) - replay, 0)]
                    yield APS(new_st, new_sh, new_la, new_rp, hs + [edge], True)
            else:
                to = Edge(to, None)
                yield APS(st, prev_sh + [to], la, rp, hs + [edge], self.reducing)

    def stable_str(self, states: typing.List[StateAndTransition], name: str = "aps") -> str:
        return """{}.stack = [{}]
{}.shift = [{}]
{}.lookahead = [{}]
{}.replay = [{}]
{}.history = [{}]
{}.reducing = {}
        """.format(
            name, " ".join(e.stable_str(states) for e in self.stack),
            name, " ".join(e.stable_str(states) for e in self.shift),
            name, ", ".join(repr(e) for e in self.lookahead),
            name, ", ".join(repr(e) for e in self.replay),
            name, " ".join(e.stable_str(states) for e in self.history),
            name, self.reducing
        )

    def string(self, name: str = "aps") -> str:
        return """{}.stack = [{}]
{}.shift = [{}]
{}.lookahead = [{}]
{}.replay = [{}]
{}.history = [{}]
{}.reducing = {}
        """.format(
            name, " ".join(str(e) for e in self.stack),
            name, " ".join(str(e) for e in self.shift),
            name, ", ".join(repr(e) for e in self.lookahead),
            name, ", ".join(repr(e) for e in self.replay),
            name, " ".join(str(e) for e in self.history),
            name, self.reducing
        )

    def __str__(self) -> str:
        return self.string()


def stable_aps_lanes_str(
        aps_lanes: typing.List[APS],
        states: typing.List[StateAndTransition],
        header: str = "lanes:",
        name: str = "\taps"
) -> str:
    return "{}\n{}".format(header, "\n".join(aps.stable_str(states, name) for aps in aps_lanes))
