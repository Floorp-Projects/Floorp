from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

import collections
import hashlib
import os
import pickle
import typing
import itertools

from . import types
from .utils import consume, keep_until, split, default_id_dict, default_fwd_dict
from .ordered import OrderedSet, OrderedFrozenSet
from .actions import Action, Replay, Reduce, FilterStates, Seq
from .grammar import End, ErrorSymbol, InitNt, Nt
from .rewrites import CanonicalGrammar
from .lr0 import LR0Generator, Term
from .aps import APS, Edge, Path

# StateAndTransitions objects are indexed using a StateId which is an integer.
StateId = int

# Action or ordered sequence of action which have to be performed.
DelayedAction = typing.Union[Action, typing.Tuple[Action, ...]]


class StateAndTransitions:
    """This is one state of the parse table, which has transitions based on
    terminals (text), non-terminals (grammar rules) and epsilon (reduce).

    In this model epsilon transitions are used to represent code to be executed
    such as reduce actions and any others actions.
    """

    __slots__ = ["index", "locations", "terminals", "nonterminals", "errors",
                 "epsilon", "delayed_actions", "arguments", "backedges", "_hash",
                 "stable_hash"]

    # Numerical index of this state.
    index: StateId

    # The stable_str of each LRItem we could be parsing in this state: the
    # places in grammar productions that tell what we've already parsed,
    # i.e. how we got to this state.
    locations: OrderedFrozenSet[str]

    # Ordered set of Actions which are pushed to the next state after a
    # conflict.
    delayed_actions: OrderedFrozenSet[DelayedAction]

    # Number of argument of an action state.
    #
    # Instead of having action states with a non-empty replay list of terms, we
    # have a non-empty list of argument which size is described by this
    # variable.
    arguments: int

    # Outgoing edges taken when shifting terminals.
    terminals: typing.Dict[str, StateId]

    # Outgoing edges taken when shifting nonterminals after reducing.
    nonterminals: typing.Dict[Nt, StateId]

    # Error symbol transitions.
    errors: typing.Dict[ErrorSymbol, StateId]

    # List of epsilon transitions with associated actions.
    epsilon: typing.List[typing.Tuple[Action, StateId]]

    # Set of edges that lead to this state.
    backedges: OrderedSet[Edge]

    # Cached hash code. This class implements __hash__ and __eq__ in order to
    # help detect equivalent states (which must be merged, for correctness).
    _hash: int

    # A hash code computed the same way as _hash, but used only for
    # human-readable output. The stability is useful for debugging, to match
    # states across multiple runs of the parser generator.
    stable_hash: str

    def __init__(
            self,
            index: StateId,
            locations: OrderedFrozenSet[str],
            delayed_actions: OrderedFrozenSet[DelayedAction] = OrderedFrozenSet(),
            arguments: int = 0
    ) -> None:
        assert isinstance(locations, OrderedFrozenSet)
        assert isinstance(delayed_actions, OrderedFrozenSet)
        self.index = index
        self.terminals = {}
        self.nonterminals = {}
        self.errors = {}
        self.epsilon = []
        self.locations = locations
        self.delayed_actions = delayed_actions
        self.arguments = arguments
        self.backedges = OrderedSet()

        # NOTE: The hash of a state depends on its location in the LR0
        # parse-table, as well as the actions which have not yet been executed.
        def hashed_content() -> typing.Iterator[object]:
            for item in sorted(self.locations):
                yield item
                yield "\n"
            yield "delayed_actions"
            for action in self.delayed_actions:
                yield hash(action)
            yield "arguments"
            yield arguments

        self._hash = hash(tuple(hashed_content()))
        h = hashlib.md5()
        h.update("".join(map(str, hashed_content())).encode())
        self.stable_hash = h.hexdigest()[:6]

    def is_inconsistent(self) -> bool:
        "Returns True if the state transitions are inconsistent."
        # TODO: We could easily allow having a state with non-terminal
        # transition and other epsilon transitions, as the non-terminal shift
        # transitions are a form of condition based on the fact that a
        # non-terminal, produced by a reduce action is consumed by the
        # automaton.
        if len(self.terminals) + len(self.nonterminals) + len(self.errors) > 0 and len(self.epsilon) > 0:
            return True
        elif len(self.epsilon) == 1:
            if any(k.is_inconsistent() for k, s in self.epsilon):
                return True
        elif len(self.epsilon) > 1:
            if any(k.is_inconsistent() for k, s in self.epsilon):
                return True
            # NOTE: We can accept multiple conditions as epsilon transitions
            # iff they are checking the same variable with non-overlapping
            # values. This implies that we can implement these conditions as a
            # deterministic switch statement in the code emitter.
            if any(not k.is_condition() for k, s in self.epsilon):
                return True
            iterator = iter(self.epsilon)
            first, _ = next(iterator)
            if any(not first.check_same_variable(k) for k, s in iterator):
                return True
            # "type: ignore" because mypy does not see that the preceding if-statement
            # means all k.condition() actions are FilterFlags.
            pairs = itertools.combinations((k for k, s in self.epsilon), 2)
            if any(not k1.check_different_values(k2) for k1, k2 in pairs):
                return True
        else:
            try:
                self.get_error_symbol()
            except ValueError:
                return True
        return False

    def shifted_edges(self) -> typing.Iterator[
            typing.Tuple[typing.Union[str, Nt, ErrorSymbol], StateId]
    ]:
        k: Term
        s: StateId
        for k, s in self.terminals.items():
            yield (k, s)
        for k, s in self.nonterminals.items():
            yield (k, s)
        for k, s in self.errors.items():
            yield (k, s)

    def edges(self) -> typing.Iterator[typing.Tuple[Term, StateId]]:
        k: Term
        s: StateId
        for k, s in self.terminals.items():
            yield (k, s)
        for k, s in self.nonterminals.items():
            yield (k, s)
        for k, s in self.errors.items():
            yield (k, s)
        for k, s in self.epsilon:
            yield (k, s)

    def rewrite_state_indexes(
            self,
            state_map: typing.Dict[StateId, StateId]
    ) -> None:
        def apply_on_term(term: typing.Union[Term, None]) -> Term:
            assert term is not None
            if isinstance(term, Action):
                return term.rewrite_state_indexes(state_map)
            return term

        self.index = state_map[self.index]
        self.terminals = {
            k: state_map[s] for k, s in self.terminals.items()
        }
        self.nonterminals = {
            k: state_map[s] for k, s in self.nonterminals.items()
        }
        self.errors = {
            k: state_map[s] for k, s in self.errors.items()
        }
        self.epsilon = [
            (k.rewrite_state_indexes(state_map), state_map[s])
            for k, s in self.epsilon
        ]
        # We cannot have multiple identical actions jumping to different locations.
        assert len(self.epsilon) == len(set(k for k, _ in self.epsilon))
        self.backedges = OrderedSet(
            Edge(state_map[edge.src], apply_on_term(edge.term))
            for edge in self.backedges
        )

    def get_error_symbol(self) -> typing.Optional[ErrorSymbol]:
        if len(self.errors) > 1:
            raise ValueError("More than one error symbol on the same state.")
        else:
            return next(iter(self.errors), None)

    def __contains__(self, term: object) -> bool:
        if isinstance(term, Action):
            for t, s in self.epsilon:
                if t == term:
                    return True
            return False
        elif isinstance(term, Nt):
            return term in self.nonterminals
        elif isinstance(term, ErrorSymbol):
            return term in self.errors
        else:
            return term in self.terminals

    def __getitem__(self, term: Term) -> StateId:
        if isinstance(term, Action):
            for t, s in self.epsilon:
                if t == term:
                    return s
            raise KeyError(term)
        elif isinstance(term, Nt):
            return self.nonterminals[term]
        if isinstance(term, ErrorSymbol):
            return self.errors[term]
        else:
            return self.terminals[term]

    def get(self, term: Term, default: object) -> object:
        try:
            return self.__getitem__(term)
        except KeyError:
            return default

    def stable_str(self, states: typing.List[StateAndTransitions]) -> str:
        conflict = ""
        if self.is_inconsistent():
            conflict = " (inconsistent)"
        return "{}{}:\n{}".format(self.stable_hash, conflict, "\n".join([
            "\t{} --> {}".format(k, states[s].stable_hash) for k, s in self.edges()]))

    def __str__(self) -> str:
        conflict = ""
        if self.is_inconsistent():
            conflict = " (inconsistent)"
        return "{}{}:\n{}".format(self.index, conflict, "\n".join([
            "\t{} --> {}".format(k, s) for k, s in self.edges()]))

    def __eq__(self, other: object) -> bool:
        return (isinstance(other, StateAndTransitions)
                and sorted(self.locations) == sorted(other.locations)
                and sorted(self.delayed_actions) == sorted(other.delayed_actions)
                and self.arguments == other.arguments)

    def __hash__(self) -> int:
        return self._hash


DebugInfo = typing.Dict[StateId, int]


class ParseTable:
    """The parser can be represented as a matrix of state transitions where on one
    side we have the current state, and on the other we have the expected
    terminal, non-terminal or epsilon transition.

            a   b   c   A   B   C   #1   #2   #3
          +---+---+---+---+---+---+----+----+----+
      s1  |   |   |   |   |   |   |    |    |    |
      s2  |   |   |   |   |   |   |    |    |    |
      s3  |   |   |   |   |   |   |    |    |    |
       .  |   |   |   |   |   |   |    |    |    |
       .  |   |   |   |   |   |   |    |    |    |
       .  |   |   |   |   |   |   |    |    |    |
      s67 |   |   |   |   |   |   |    |    |    |
      s68 |   |   |   |   |   |   |    |    |    |
      s69 |   |   |   |   |   |   |    |    |    |
          +---+---+---+---+---+---+----+----+----+

    The terminals `a` are the token which are read from the input. The
    non-terminals `A` are the token which are pushed by the reduce actions of
    the epsilon transitions. The epsilon transitions `#1` are the actions which
    have to be executed as code by the parser.

    A parse table is inconsistent if there is any state which has an epsilon
    transitions and terminals/non-terminals transitions (shift-reduce
    conflict), or a state with more than one epsilon transitions (reduce-reduce
    conflict). This is equivalent to having a non deterministic state machine.

    """

    __slots__ = [
        "actions", "states", "state_cache", "named_goals", "terminals",
        "nonterminals", "debug_info", "exec_modes", "assume_inconsistent"
    ]

    # Map of actions identifier to the corresponding object.
    actions: typing.List[Action]

    # Map of state identifier to the corresponding object.
    states: typing.List[StateAndTransitions]

    # Hash table of state objects, ensuring we never have two equal states.
    state_cache: typing.Dict[StateAndTransitions, StateAndTransitions]

    # List of (Nt, states) tuples which are the entry point of the state
    # machine.
    named_goals: typing.List[typing.Tuple[Nt, StateId]]

    # Set of all terminals.
    terminals: OrderedFrozenSet[typing.Union[str, End]]

    # List of non-terminals.
    nonterminals: typing.List[Nt]

    # Carry the info to be used when generating debug_context. If False,
    # then no debug_context is ever produced.
    debug_info: typing.Union[bool, DebugInfo]

    # Execution modes are used by the code generator to decide which
    # function is executed when. This is a dictionary of OrderedSet, where
    # the keys are the various parsing modes, and the mapped set contains
    # the list of traits which have to be implemented, and consequently
    # which functions would be encoded.
    exec_modes: typing.Optional[typing.DefaultDict[str, OrderedSet[types.Type]]]

    # True if the parse table might be inconsistent. When this is False, we add
    # extra assertions when computing the reduce path.
    assume_inconsistent: bool

    def __init__(
            self,
            grammar: CanonicalGrammar,
            verbose: bool = False,
            progress: bool = False,
            debug: bool = False
    ) -> None:
        self.actions = []
        self.states = []
        self.state_cache = {}
        self.named_goals = []
        self.terminals = grammar.grammar.terminals
        self.nonterminals = typing.cast(
            typing.List[Nt],
            list(grammar.grammar.nonterminals.keys()))

        # typing.cast() doesn't actually check at run time, so let's do that:
        assert all(isinstance(nt, Nt) for nt in self.nonterminals)

        self.debug_info = debug
        self.exec_modes = grammar.grammar.exec_modes
        self.assume_inconsistent = True
        self.create_lr0_table(grammar, verbose, progress)
        self.fix_inconsistent_table(verbose, progress)
        # TODO: Optimize chains of actions into sequences.
        # Optimize by removing unused states.
        self.remove_all_unreachable_state(verbose, progress)
        # TODO: Statically compute replayed terms. (maybe?)
        # Replace reduce actions by programmatic stack manipulation.
        self.lower_reduce_actions(verbose, progress)
        # Fold Replay followed by Unwind instruction.
        self.fold_replay_unwind(verbose, progress)
        # Fold paths which have the same ending.
        self.fold_identical_endings(verbose, progress)
        # Group state with similar non-terminal edges close-by, to improve the
        # generated Rust code by grouping matched state numbers.
        self.group_nonterminal_states(verbose, progress)
        # Split shift states from epsilon states.
        # self.group_epsilon_states(verbose, progress)

    def save(self, filename: os.PathLike) -> None:
        with open(filename, 'wb') as f:
            pickle.dump(self, f)

    @classmethod
    def load(cls, filename: os.PathLike) -> ParseTable:
        with open(filename, 'rb') as f:
            obj = pickle.load(f)
            if len(f.read()) != 0:
                raise ValueError("file has unexpected extra bytes at end")
        if not isinstance(obj, cls):
            raise TypeError("file contains wrong kind of object: expected {}, got {}"
                            .format(cls.__name__, obj.__class__.__name__))
        return obj

    def is_inconsistent(self) -> bool:
        "Returns True if the grammar contains any inconsistent state."
        for s in self.states:
            if s is not None and s.is_inconsistent():
                return True
        return False

    def rewrite_state_indexes(self, state_map: typing.Dict[StateId, StateId]) -> None:
        for s in self.states:
            if s is not None:
                s.rewrite_state_indexes(state_map)
        self.named_goals = [
            (nt, state_map[s]) for nt, s in self.named_goals
        ]

        # After a rewrite, multiple actions (conditions) might jump to the same
        # target, attempt to fold these conditions based on having the same
        # target. If we can merge them, then remove previous edges (updating
        # the backedges of successor states) and replace them by the newly
        # created edges.
        for s in self.states:
            if s is not None and len(s.epsilon) != 0:
                epsilon_by_dest = collections.defaultdict(list)
                for k, d in s.epsilon:
                    epsilon_by_dest[d].append(k)
                for d, ks in epsilon_by_dest.items():
                    if len(ks) == 1:
                        continue
                    new_ks = ks[0].fold_by_destination(ks)
                    if new_ks == ks:
                        continue
                    # This collection is required by `remove_edge`, but in this
                    # particular case we know for sure that at least one edge
                    # would be added back. Therefore no need to use the content
                    # of the set.
                    maybe_unreachable_set: OrderedSet[StateId] = OrderedSet()
                    assert len(new_ks) > 0
                    for k in ks:
                        self.remove_edge(s, k, maybe_unreachable_set)
                    for k in new_ks:
                        self.add_edge(s, k, d)

        self.assert_table_invariants()

    def rewrite_reordered_state_indexes(self) -> None:
        state_map = {
            s.index: i
            for i, s in enumerate(self.states)
            if s is not None
        }
        self.rewrite_state_indexes(state_map)

    def new_state(
            self,
            locations: OrderedFrozenSet[str],
            delayed_actions: OrderedFrozenSet[DelayedAction] = OrderedFrozenSet(),
            arguments: int = 0
    ) -> typing.Tuple[bool, StateAndTransitions]:
        """Get or create state with an LR0 location and delayed actions. Returns a tuple
        where the first element is whether the element is newly created, and
        the second element is the State object."""
        index = len(self.states)
        state = StateAndTransitions(index, locations, delayed_actions, arguments)
        try:
            return False, self.state_cache[state]
        except KeyError:
            self.state_cache[state] = state
            self.states.append(state)
            return True, state

    def get_state(
            self,
            locations: OrderedFrozenSet[str],
            delayed_actions: OrderedFrozenSet[DelayedAction] = OrderedFrozenSet(),
            arguments: int = 0
    ) -> StateAndTransitions:
        """Like new_state(), but only returns the state without returning whether it is
        newly created or not."""
        _, state = self.new_state(locations, delayed_actions, arguments)
        return state

    def remove_state(self, s: StateId, maybe_unreachable_set: OrderedSet[StateId]) -> None:
        state = self.states[s]
        self.clear_edges(state, maybe_unreachable_set)
        del self.state_cache[state]

        # "type: ignore" because the type annotation on `states` doesn't allow
        # entries to be `None`.
        self.states[s] = None  # type: ignore

    def add_edge(
            self,
            src: StateAndTransitions,
            term: Term,
            dest: StateId
    ) -> None:
        assert term not in src
        assert dest < len(self.states)
        if isinstance(term, Action):
            src.epsilon.append((term, dest))
        elif isinstance(term, Nt):
            src.nonterminals[term] = dest
        elif isinstance(term, ErrorSymbol):
            src.errors[term] = dest
        else:
            src.terminals[term] = dest
        self.states[dest].backedges.add(Edge(src.index, term))

    def remove_backedge(
            self,
            src: StateAndTransitions,
            term: Term,
            dest: StateId,
            maybe_unreachable_set: OrderedSet[StateId]
    ) -> None:
        self.states[dest].backedges.remove(Edge(src.index, term))
        maybe_unreachable_set.add(dest)

    def replace_edge(
            self,
            src: StateAndTransitions,
            term: Term,
            dest: StateId,
            maybe_unreachable_set: OrderedSet[StateId]
    ) -> None:
        assert isinstance(dest, int) and dest < len(self.states)

        edge_existed = term in src
        if edge_existed:
            old_dest = src[term]
            self.remove_backedge(src, term, old_dest, maybe_unreachable_set)

        if isinstance(term, Action):
            src.epsilon = [(t, d) for t, d in src.epsilon if t != term]
            src.epsilon.append((term, dest))
        elif isinstance(term, Nt):
            src.nonterminals[term] = dest
        elif isinstance(term, ErrorSymbol):
            src.errors[term] = dest
        else:
            src.terminals[term] = dest
        self.states[dest].backedges.add(Edge(src.index, term))

        self.assert_state_invariants(src)
        self.assert_state_invariants(dest)
        if edge_existed:
            self.assert_state_invariants(old_dest)

    def remove_edge(
            self,
            src: StateAndTransitions,
            term: Term,
            maybe_unreachable_set: OrderedSet[StateId]
    ) -> None:
        edge_existed = term in src
        if edge_existed:
            old_dest = src[term]
            self.remove_backedge(src, term, old_dest, maybe_unreachable_set)
        if isinstance(term, Action):
            src.epsilon = [(t, d) for t, d in src.epsilon if t != term]
        elif isinstance(term, Nt):
            del src.nonterminals[term]
        elif isinstance(term, ErrorSymbol):
            del src.errors[term]
        else:
            del src.terminals[term]

        self.assert_state_invariants(src)
        if edge_existed:
            self.assert_state_invariants(old_dest)

    def clear_edges(
            self,
            src: StateAndTransitions,
            maybe_unreachable_set: OrderedSet[StateId]
    ) -> None:
        """Remove all existing edges, in order to replace them by new one. This is used
        when resolving shift-reduce conflicts."""
        assert isinstance(src, StateAndTransitions)
        old_dest = []
        for term, dest in src.edges():
            self.remove_backedge(src, term, dest, maybe_unreachable_set)
            old_dest.append(dest)
        src.terminals = {}
        src.nonterminals = {}
        src.errors = {}
        src.epsilon = []
        self.assert_state_invariants(src)
        for dest in old_dest:
            self.assert_state_invariants(dest)

    def assert_table_invariants(self) -> None:
        for s in self.states:
            if s is not None:
                self.assert_state_invariants(s)

    def assert_state_invariants(self, src: typing.Union[StateId, StateAndTransitions]) -> None:
        if not self.debug_info:
            return
        if isinstance(src, int):
            src = self.states[src]
        assert isinstance(src, StateAndTransitions)
        try:
            for term, dest in src.edges():
                assert Edge(src.index, term) in self.states[dest].backedges
            for e in src.backedges:
                assert e.term is not None
                assert self.states[e.src][e.term] == src.index
            if not self.assume_inconsistent:
                assert not src.is_inconsistent()
        except AssertionError as exc:
            print("assert_state_inveriants for {}\n".format(src))
            for e in src.backedges:
                print("backedge {} from {}\n".format(e, self.states[e.src]))
            raise exc

    def remove_unreachable_states(
            self,
            maybe_unreachable_set: OrderedSet[StateId]
    ) -> None:
        # TODO: This function is incomplete in case of loops, some cycle might
        # remain isolated while not being reachable from the init states. We
        # should maintain a notion of depth per-state, such that we can
        # identify loops by noticing the all backedges have a larger depth than
        # the current state.
        init: OrderedSet[StateId]
        init = OrderedSet(goal for name, goal in self.named_goals)
        while maybe_unreachable_set:
            next_set: OrderedSet[StateId] = OrderedSet()
            for s in maybe_unreachable_set:
                # Check if the state is reachable, if not remove the state and
                # fill the next_set with all outgoing edges.
                if len(self.states[s].backedges) == 0 and s not in init:
                    self.remove_state(s, next_set)
            maybe_unreachable_set = next_set

    def is_reachable_state(self, s: StateId) -> bool:
        """Check whether the current state is reachable or not."""
        if self.states[s] is None:
            return False
        reachable_back: OrderedSet[StateId] = OrderedSet()
        todo = [s]
        while todo:
            s = todo.pop()
            reachable_back.add(s)
            for edge in self.states[s].backedges:
                if edge.src not in reachable_back:
                    todo.append(edge.src)
        for _, s in self.named_goals:
            if s in reachable_back:
                return True
        return False

    def debug_dump(self) -> None:
        # Sort the grammar by state hash, such that it can be compared
        # before/after grammar modifications.
        temp = [s for s in self.states if s is not None]
        temp = sorted(temp, key=lambda s: s.stable_hash)
        for s in temp:
            print(s.stable_str(self.states))

    def create_lr0_table(
            self,
            grammar: CanonicalGrammar,
            verbose: bool,
            progress: bool
    ) -> None:
        if verbose or progress:
            print("Create LR(0) parse table.")

        goals = grammar.grammar.goals()
        self.named_goals = []

        # Temporary work queue.
        todo: typing.Deque[typing.Tuple[LR0Generator, StateAndTransitions]]
        todo = collections.deque()

        # Record the starting goals in the todo list.
        for nt in goals:
            init_nt = Nt(InitNt(nt), ())
            it = LR0Generator.start(grammar, init_nt)
            s = self.get_state(it.stable_locations())
            todo.append((it, s))
            self.named_goals.append((nt, s.index))

        # Iterate the grammar with sets of LR Items abstracted by the
        # LR0Generator, and create new states in the parse table as long as new
        # sets of LR Items are discovered.
        def visit_grammar() -> typing.Iterator[None]:
            while todo:
                yield  # progress bar.
                # TODO: Compare stack / queue, for the traversal of the states.
                s_it, s = todo.popleft()
                if verbose:
                    print("\nMapping state {} to LR0:\n{}".format(s.stable_hash, s_it))
                for k, sk_it in s_it.transitions().items():
                    locations = sk_it.stable_locations()
                    if not self.term_is_shifted(k):
                        locations = OrderedFrozenSet()
                    is_new, sk = self.new_state(locations)
                    if is_new:
                        todo.append((sk_it, sk))

                    # Add the edge from s to sk with k.
                    self.add_edge(s, k, sk.index)

        consume(visit_grammar(), progress)

        if verbose:
            print("Create LR(0) Table Result:")
            self.debug_dump()

    def term_is_shifted(self, term: typing.Optional[Term]) -> bool:
        return not isinstance(term, Action) or term.follow_edge()

    def is_valid_path(
            self,
            path: typing.Sequence[Edge],
            state: typing.Optional[StateId] = None
    ) -> bool:
        """This function is used to check a list of edges and returns whether it
        corresponds to a valid path within the parse table. This is useful when
        merging sequences of edges from various locations."""
        if not state and path != []:
            state = path[0].src
        while path:
            edge = path[0]
            path = path[1:]
            if state != edge.src:
                return False
            assert isinstance(state, StateId)

            term = edge.term
            if term is None and len(path) == 0:
                return True

            row = self.states[state]
            if term not in row:
                return False
            assert term is not None
            state = row[term]
        return True

    def term_is_stacked(self, term: typing.Optional[Term]) -> bool:
        # The `term` argument is annotated as Optional because `Edge.term` is a
        # common argument. If it's ever None in practice, the caller has a bug.
        assert term is not None

        return not isinstance(term, Action)

    def aps_visitor(self, aps: APS, visit: typing.Callable[[APS], bool]) -> None:
        """Visit all the states of the parse table, as-if we were running a
        Generalized LR parser.

        However, instead parsing content, we use this algorithm to generate
        both the content which remains to be parsed as well as the context
        which might lead us to be in the state which from which we started.

        This algorithm takes an APS (Abstract Parser State) and a callback, and
        consider all edges of the parse table, unless restricted by one of the
        previously encountered actions. These restrictions, such as replayed
        lookahead or the path which might be reduced are used for filtering out
        states which are not handled by this parse table.

        For each edge, this functions calls the visit functions to know whether
        to stop or continue. The visit function might capture APS given as
        argument to be used for other analysis.

        """
        todo = [aps]
        while todo:
            aps = todo.pop()
            cont = visit(aps)
            if not cont:
                continue
            todo.extend(aps.shift_next(self))

    def context_lanes(self, state: StateId) -> typing.Tuple[bool, typing.List[APS]]:
        """Compute lanes, such that each reduce action can have set of unique stack to
        reach the given state. The stacks are assumed to be loop-free by
        reducing edges at most once.

        In order to avoid attempting to eagerly solve everything using context
        information, we break this loop as soon as we have one token of
        lookahead in a case which does not have enough context.

        The return value is a tuple where the first element is a boolean which
        is True if we should fallback on solving this issue with more
        lookahead, and the second is the list of APS lanes which are providing
        enough context to disambiguate the inconsistency of the given state."""

        def not_interesting(aps: APS) -> bool:
            reduce_list = [e for e in aps.history if self.term_is_shifted(e.term)]
            has_reduce_loop = len(reduce_list) != len(set(reduce_list))
            return has_reduce_loop

        # The context is a dictionary which maps all stack suffixes from an APS
        # stack. It is mapped to a list of tuples, where the each tuple is the
        # index with the APS stack and the APS action used to follow this path.
        context: typing.DefaultDict[typing.Tuple[Edge, ...], typing.List[Edge]]
        context = collections.defaultdict(lambda: [])

        def has_enough_context(aps: APS) -> bool:
            try:
                assert aps.history[0] in context[tuple(aps.stack)]
                # Check the number of different actions which can reach this
                # location. If there is more than 1, then we do not have enough
                # context.
                return len(set(context[tuple(aps.stack)])) <= 1
            except IndexError:
                return False

        collect = [APS.start(state)]
        enough_context = False
        while not enough_context:
            # print("collect.len = {}".format(len(collect)))
            # Fill the context dictionary with all the sub-stack which might be
            # encountered by other APS.
            recurse = []
            context = collections.defaultdict(lambda: [])
            while collect:
                aps = collect.pop()
                recurse.append(aps)
                if aps.history == []:
                    continue
                for i in range(len(aps.stack)):
                    context[tuple(aps.stack[i:])].append(aps.history[0])
            assert collect == []

            # print("recurse.len = {}".format(len(recurse)))
            # Iterate over APS which do not yet have enough context information
            # to uniquely identify a single action.
            enough_context = True
            while recurse:
                aps = recurse.pop()
                if not_interesting(aps):
                    # print("discard uninteresting context lane:")
                    # print(aps.string("\tcontext"))
                    continue
                if has_enough_context(aps):
                    collect.append(aps)
                    continue
                # If we have not enough context but some lookahead is
                # available, attempt to first solve this issue using more
                # lookahead before attempting to use context information.
                if len(aps.lookahead) >= 1:
                    # print("discard context_lanes due to lookahead:")
                    # for aps in itertools.chain(collect, recurse, [aps]):
                    #     print(aps.string("\tcontext"))
                    return True, []
                enough_context = False
                # print("extend starting at:\n{}".format(aps.string("\tcontext")))
                collect.extend(aps.shift_next(self))
            assert recurse == []

        # print("context_lanes:")
        # for aps in collect:
        #     print(aps.string("\tcontext"))

        return False, collect

    def lookahead_lanes(self, state: StateId) -> typing.List[APS]:
        """Compute lanes to collect all lookahead symbols available. After each reduce
        action, there is no need to consider the same non-terminal multiple
        times, we are only interested in lookahead token and not in the context
        information provided by reducing action."""

        record = []
        # After the first reduce action, we do not want to spend too much
        # resource visiting edges which would give us the same information.
        # Therefore, if we already reduce an action to a given state, then we
        # skip looking for lookahead that we already visited.
        #
        # Set of (first-reduce-edge, reducing-base, last-reduce-edge)
        seen_edge_after_reduce: typing.Set[typing.Tuple[Edge, StateId, typing.Optional[Term]]]
        seen_edge_after_reduce = set()

        def find_first_reduce(
                edges: Path
        ) -> typing.Tuple[int, typing.Optional[Edge]]:
            for i, edge in enumerate(edges):
                if not self.term_is_shifted(edge.term):
                    return i, edge
            return 0, None

        def find_last_reduce(
                edges: Path
        ) -> typing.Tuple[int, typing.Optional[Edge]]:
            for i, edge in zip(reversed(range(len(edges))), reversed(edges)):
                if not self.term_is_shifted(edge.term):
                    return i, edge
            return 0, None

        def visit(aps: APS) -> bool:
            # Note, this suppose that we are not considering flags when
            # computing, as flag might prevent some lookahead investigations.
            reduce_key = None
            first_index, first_reduce = find_first_reduce(aps.history)
            last_index, last_reduce = find_last_reduce(aps.history)
            if first_index != last_index and first_reduce and last_reduce:
                if not isinstance(aps.history[-1].term, Action):
                    reduce_key = (first_reduce, aps.shift[0].src, last_reduce.term)
            has_seen_edge_after_reduce = reduce_key and reduce_key in seen_edge_after_reduce
            has_lookahead = len(aps.lookahead) >= 1
            stop = has_seen_edge_after_reduce or has_lookahead
            # print("stop: {}, size lookahead: {}, seen_edge_after_reduce: {}".format(
            #     stop, len(aps.lookahead), repr(reduce_key)
            # ))
            # print(aps.string("\tvisitor"))
            if stop:
                if has_lookahead:
                    record.append(aps)
            if reduce_key:
                seen_edge_after_reduce.add(reduce_key)
            return not stop

        self.aps_visitor(APS.start(state), visit)
        return record

    def fix_with_context(self, s: StateId, aps_lanes: typing.List[APS]) -> None:
        raise ValueError("fix_with_context: Not Implemented")
    #     # This strategy is about using context information. By using chains of
    #     # reduce actions, we are able to increase the knowledge of the stack
    #     # content. The stack content is the context which can be used to
    #     # determine how to consider a reduction. The stack content is also
    #     # called a lane, as defined in the Lane Table algorithm.
    #     #
    #     # To add context information to the current graph, we add flags
    #     # manipulation actions.
    #     #
    #     # Consider each edge as having an implicit function which can map one
    #     # flag value to another. The following implements a unification
    #     # algorithm which is attempting to solve the question of what is the
    #     # flag value, and where it should be changed.
    #     #
    #     # NOTE: (nbp) I would not be surprised if there is a more specialized
    #     # algorithm, but I failed to find one so far, and this problem
    #     # definitely looks like a unification problem.
    #     Id = collections.namedtuple("Id", "edge")
    #     Eq = collections.namedtuple("Eq", "flag_in edge flag_out")
    #     Var = collections.namedtuple("Var", "n")
    #     SubSt = collections.namedtuple("SubSt", "var by")
    #
    #     # Unify expression, and return one substitution if both expressions
    #     # can be unified.
    #     def unify_expr(expr1, expr2, swapped=False):
    #         if isinstance(expr1, Eq) and isinstance(expr2, Id):
    #             if expr1.edge != expr2.edge:
    #                 # Different edges are ok, but produce no substituions.
    #                 return True
    #             if isinstance(expr1.flag_in, Var):
    #                 return SubSt(expr1.flag_in, expr1.flag_out)
    #             if isinstance(expr1.flag_out, Var):
    #                 return SubSt(expr1.flag_out, expr1.flag_in)
    #             # We are unifying with a relation which consider the current
    #             # function as an identity function. Having different values as
    #             # input and output fails the unification rule.
    #             return expr1.flag_out == expr1.flag_in
    #         if isinstance(expr1, Eq) and isinstance(expr2, Eq):
    #             if expr1.edge != expr2.edge:
    #                 # Different edges are ok, but produce no substituions.
    #                 return True
    #             if expr1.flag_in is None and isinstance(expr2.flag_in, Var):
    #                 return SubSt(expr2.flag_in, None)
    #             if expr1.flag_out is None and isinstance(expr2.flag_out, Var):
    #                 return SubSt(expr2.flag_out, None)
    #             if expr1.flag_in == expr2.flag_in:
    #                 if isinstance(expr1.flag_out, Var):
    #                     return SubSt(expr1.flag_out, expr2.flag_out)
    #                 elif isinstance(expr2.flag_out, Var):
    #                     return SubSt(expr2.flag_out, expr1.flag_out)
    #                 # Reject solutions which are not deterministic. We do not
    #                 # want the same input flag to have multiple outputs.
    #                 return expr1.flag_out == expr2.flag_out
    #             if expr1.flag_out == expr2.flag_out:
    #                 if isinstance(expr1.flag_in, Var):
    #                     return SubSt(expr1.flag_in, expr2.flag_in)
    #                 elif isinstance(expr2.flag_in, Var):
    #                     return SubSt(expr2.flag_in, expr1.flag_in)
    #                 return True
    #         if not swapped:
    #             return unify_expr(expr2, expr1, True)
    #         return True
    #
    #     # Apply substituion rule to an expression.
    #     def subst_expr(subst, expr):
    #         if expr == subst.var:
    #             return True, subst.by
    #         if isinstance(expr, Eq):
    #             subst1, flag_in = subst_expr(subst, expr.flag_in)
    #             subst2, flag_out = subst_expr(subst, expr.flag_out)
    #             return subst1 or subst2, Eq(flag_in, expr.edge, flag_out)
    #         return False, expr
    #
    #     # Add an expression to an existing knowledge based which is relying on
    #     # a set of free variables.
    #     def unify_with(expr, knowledge, free_vars):
    #         old_knowledge = knowledge
    #         old_free_Vars = free_vars
    #         while True:
    #             subst = None
    #             for rel in knowledge:
    #                 subst = unify_expr(rel, expr)
    #                 if subst is False:
    #                     raise Error("Failed to find a coherent solution")
    #                 if subst is True:
    #                     continue
    #                 break
    #             else:
    #                 return knowledge + [expr], free_vars
    #             free_vars = [fv for fv in free_vars if fv != subst.var]
    #             # Substitue variables, and re-add rules which have substituted
    #             # vars to check the changes to other rules, in case 2 rules are
    #             # now in conflict or in case we can propagate more variable
    #             # changes.
    #             subst_rules = [subst_expr(subst, k) for k in knowledge]
    #             knowledge = [rule for changed, rule in subst_rule if not changed]
    #             for changed, rule in subst_rule:
    #                 if not changed:
    #                     continue
    #                 knowledge, free_vars = unify_with(rule, knowledge, free_vars)
    #
    #     # Register boundary conditions as part of the knowledge based, i-e that
    #     # reduce actions are expecting to see the flag value matching the
    #     # reduced non-terminal, and that we have no flag value at the start of
    #     # every lane head.
    #     #
    #     # TODO: Catch exceptions from the unify function in case we do not yet
    #     # have enough context to disambiguate.
    #     rules = []
    #     free_vars = []
    #     last_free = 0
    #     maybe_id_edges = set()
    #     nts = set()
    #     for aps in aps_lanes:
    #         assert len(aps.stack) >= 1
    #         flag_in = None
    #         for edge in aps.stack[-1]:
    #             i = last_free
    #             last_free += 1
    #             free_vars.append(Var(i))
    #             rule = Eq(flag_in, edge, Var(i))
    #             rules, free_vars = unify_with(rule, rules, free_vars)
    #             flag_in = Var(i)
    #             if flag_in is not None:
    #                 maybe_id_edges.add(Id(edge))
    #         edge = aps.stack[-1]
    #         nt = edge.term.update_stack_with().nt
    #         rule = Eq(nt, edge, None)
    #         rules, free_vars = unify_with(rule, rules, free_vars)
    #         nts.add(nt)
    #
    #     # We want to produce a parse table where most of the node are ignoring
    #     # the content of the flag which is being added. Thus we want to find a
    #     # solution where most edges are the identical function.
    #     def fill_with_id_functions(rules, free_vars, maybe_id_edges):
    #         min_rules, min_vars = rules, free_vars
    #         for num_id_edges in reversed(range(len(maybe_id_edges))):
    #             for id_edges in itertools.combinations(edges, num_id_edges):
    #                 for edge in id_edges:
    #                     new_rules, new_free_vars = unify_with(rule, rules, free_vars)
    #                     if new_free_vars == []:
    #                         return new_rules, new_free_vars
    #                     if len(new_free_vars) < len(min_free_vars):
    #                         min_vars = new_free_vars
    #                         min_rules = new_rules
    #         return rules, free_vars
    #
    #     rules, free_vars = fill_with_id_functions(rules, free_vars, maybe_id_edges)
    #     if free_vars != []:
    #         raise Error("Hum … maybe we can try to iterate over the remaining free-variable.")
    #     print("debug: Great we found a solution for a reduce-reduce conflict")
    #
    #     # The set of rules describe the function that each edge is expected to
    #     # support. If there is an Id(edge), then we know that we do not have to
    #     # change the graph for the given edge. If the rule is Eq(A, edge, B),
    #     # then we have to (filter A & pop) and push B, except if A or B is
    #     # None.
    #     #
    #     # For each edge, collect the set of rules concerning the edge to
    #     # determine which edges have to be transformed to add the filter&pop
    #     # and push actions.
    #     edge_rules = collections.defaultdict(lambda: [])
    #     for rule in rules:
    #         if isinstance(rule, Id):
    #             edge_rules[rule.edge] = None
    #         elif isinstance(rule, Eq):
    #             if edge_rules[rule.edge] is not None:
    #                 edge_rules[rule.edge].append(rule)
    #
    #     maybe_unreachable_set = set()
    #     flag_name = self.get_flag_for(nts)
    #     for edge, rules in edge_rules.items():
    #         # If the edge is an identity function, then skip doing any
    #         # modifications on it.
    #         if rules is None:
    #             continue
    #         # Otherwise, create a new state and transition for each mapping.
    #         src = self.states[edge.src]
    #         dest = src[edge.term]
    #         dest_state = self.states[dest]
    #         # TODO: Add some information to avoid having identical hashes as
    #         # the destination.
    #         actions = []
    #         for rule in OrderedFrozenSet(rules):
    #             assert isinstance(rule, Eq)
    #             seq = []
    #             if rule.flag_in is not None:
    #                 seq.append(FilterFlag(flag_name, True))
    #                 if rule.flag_in != rule.flag_out:
    #                     seq.append(PopFlag(flag_name))
    #             if rule.flag_out is not None and rule.flag_in != rule.flag_out:
    #                 seq.append(PushFlag(flag_name, rule.flag_out))
    #             actions.append(Seq(seq))
    #         # Assert that we do not map flag_in more than once.
    #         assert len(set(eq.flag_in for eq in rules)) < len(rules)
    #         # Create the new state and add edges.
    #         is_new, switch = self.new_state(dest.locations, OrderedFrozenSet(actions))
    #         assert is_new
    #         for seq in actions:
    #             self.add_edge(switch, seq, dest)
    #
    #         # Replace the edge from src to dest, by an edge from src to the
    #         # newly created switch state, which then decide which flag to set
    #         # before going to the destination target.
    #         self.replace_edge(src, edge.term, switch, maybe_unreachable_set)
    #
    #     self.remove_unreachable_states(maybe_unreachable_set)
    #     pass

    def fix_with_lookahead(self, s: StateId, aps_lanes: typing.List[APS]) -> None:
        # Find the list of terminals following each actions (even reduce
        # actions).
        assert all(len(aps.lookahead) >= 1 for aps in aps_lanes)
        if self.debug_info:
            for aps in aps_lanes:
                print(str(aps))
        maybe_unreachable_set: OrderedSet[StateId] = OrderedSet()

        # For each shifted term, associate a set of state and actions which
        # would have to be executed.
        shift_map: typing.DefaultDict[
            Term,
            typing.List[typing.Tuple[StateAndTransitions, typing.List[Edge]]]
        ]
        shift_map = collections.defaultdict(lambda: [])
        for aps in aps_lanes:
            actions = aps.history
            assert isinstance(actions[-1], Edge)
            src = actions[-1].src
            term = actions[-1].term
            assert term == aps.lookahead[0]
            assert isinstance(term, (str, End, ErrorSymbol, Nt))

            # No need to consider any action beyind the first reduced action
            # since the reduced action is in charge of replaying the lookahead
            # terms.
            actions = list(keep_until(actions[:-1], lambda edge: not self.term_is_shifted(edge.term)))
            assert all(isinstance(edge.term, Action) for edge in actions)

            # Change the order of the shifted term, shift all actions by 1 with
            # the given lookahead term, in order to match the newly generated
            # state machine.
            #
            # Shifting actions with the list of shifted terms is used to record
            # the number of terms to be replayed, as well as verifying whether
            # Lookahead filter actions should accept or reject this lane.
            new_actions = []
            accept = True
            for edge in actions:
                edge_term = edge.term
                assert isinstance(edge_term, Action)
                new_term = edge_term.shifted_action(term)
                if isinstance(new_term, bool):
                    if new_term is False:
                        accept = False
                        break
                    else:
                        continue
                new_actions.append(Edge(edge.src, new_term))
            if accept:
                target_id = self.states[src][term]
                target = self.states[target_id]
                shift_map[term].append((target, new_actions))

        # Restore the new state machine based on a given state to use as a base
        # and the shift_map corresponding to edges.
        def restore_edges(
                state: StateAndTransitions,
                shift_map: typing.DefaultDict[
                    Term,
                    typing.List[typing.Tuple[StateAndTransitions, typing.List[Edge]]]
                ],
                depth: str
        ) -> None:
            # print("{}starting with {}\n".format(depth, state))
            edges = {}
            for term, actions_list in shift_map.items():
                # print("{}term: {}, lists: {}\n".format(depth, repr(term), repr(actions_list)))
                # Collect all the states reachable after shifting the term.
                # Compute the unique name, based on the locations and actions
                # which are delayed.
                locations: OrderedSet[str] = OrderedSet()
                delayed: OrderedSet[DelayedAction] = OrderedSet()
                new_shift_map: typing.DefaultDict[
                    Term,
                    typing.List[typing.Tuple[StateAndTransitions, typing.List[Edge]]]
                ]
                new_shift_map = collections.defaultdict(lambda: [])
                recurse = False
                if not self.term_is_shifted(term):
                    # There is no more target after a reduce action.
                    actions_list = []
                for target, actions in actions_list:
                    assert isinstance(target, StateAndTransitions)
                    locations |= target.locations
                    delayed |= target.delayed_actions
                    if actions != []:
                        # Pull edges, with delayed actions.
                        edge = actions[0]
                        assert isinstance(edge, Edge)
                        for action in actions:
                            action_term = action.term
                            assert isinstance(action_term, Action)
                            delayed.add(action_term)
                        edge_term = edge.term
                        assert edge_term is not None
                        new_shift_map[edge_term].append((target, actions[1:]))
                        recurse = True
                    else:
                        # Pull edges, as a copy of existing edges.
                        for next_term, next_dest_id in target.edges():
                            next_dest = self.states[next_dest_id]
                            new_shift_map[next_term].append((next_dest, []))

                is_new, new_target = self.new_state(
                    OrderedFrozenSet(locations), OrderedFrozenSet(delayed))
                edges[term] = new_target.index
                if self.debug_info:
                    print("{}is_new = {}, index = {}".format(depth, is_new, new_target.index))
                    print("{}Add: {} -- {} --> {}".format(depth, state.index, str(term), new_target.index))
                    print("{}continue: (is_new: {}) or (recurse: {})".format(depth, is_new, recurse))
                if is_new or recurse:
                    restore_edges(new_target, new_shift_map, depth + "  ")

            self.clear_edges(state, maybe_unreachable_set)
            for term, target_id in edges.items():
                self.add_edge(state, term, target_id)
            if self.debug_info:
                print("{}replaced by {}\n".format(depth, state))

        state = self.states[s]
        restore_edges(state, shift_map, "")
        self.remove_unreachable_states(maybe_unreachable_set)

    def fix_inconsistent_state(self, s: StateId, verbose: bool) -> bool:
        # Fix inconsistent states works one state at a time. The goal is to
        # achieve the same method as the Lane Tracer, but instead of building a
        # table to then mutate the parse state, we mutate the parse state
        # directly.
        #
        # This strategy is simpler, and should be able to reproduce the same
        # graph mutations as seen with Lane Table algorithm. One of the problem
        # with the Lane Table algorithm is that it assume reduce operations,
        # and as such it does not apply simply to epsilon transitions which are
        # used as conditions on the parse table.
        #
        # By using push-flag and filter-flag actions, we are capable to
        # decompose the Lane Table transformation of the parse table into
        # multiple steps which are working one step at a time, and with less
        # table state duplication.

        state = self.states[s]
        if state is None or not state.is_inconsistent():
            return False

        all_reduce = all(a.update_stack() for a, _ in state.epsilon)
        any_shift = (len(state.terminals) + len(state.nonterminals) + len(state.errors)) > 0
        try_with_context = all_reduce and not any_shift
        try_with_lookahead = not try_with_context
        # if verbose:
        #     print(aps_lanes_str(aps_lanes, "fix_inconsistent_state:", "\taps"))
        if try_with_context:
            if verbose:
                print("\tFix with context.")
            try_with_lookahead, aps_lanes = self.context_lanes(s)
            if not try_with_lookahead:
                assert aps_lanes != []
                self.fix_with_context(s, aps_lanes)
            elif verbose:
                print("\tFallback on fixing with lookahead.")
        if try_with_lookahead:
            if verbose:
                print("\tFix with lookahead.")
            aps_lanes = self.lookahead_lanes(s)
            assert aps_lanes != []
            self.fix_with_lookahead(s, aps_lanes)
        return True

    def fix_inconsistent_table(self, verbose: bool, progress: bool) -> None:
        """The parse table might be inconsistent. We fix the parse table by looking
        around the inconsistent states for more context. Either by looking at the
        potential stack state which might lead to the inconsistent state, or by
        increasing the lookahead."""
        self.assume_inconsistent = True
        if verbose or progress:
            print("Fix parse table inconsistencies.")

        todo: typing.Deque[StateId] = collections.deque()
        for state in self.states:
            if state.is_inconsistent():
                todo.append(state.index)

        if verbose and todo:
            print("\n".join([
                "\nGrammar is inconsistent.",
                "\tNumber of States = {}",
                "\tNumber of inconsistencies found = {}"]).format(
                    len(self.states), len(todo)))

        count = 0

        def visit_table() -> typing.Iterator[None]:
            nonlocal count
            unreachable = []
            while todo:
                while todo:
                    yield  # progress bar.
                    # TODO: Compare stack / queue, for the traversal of the states.
                    s = todo.popleft()
                    if not self.is_reachable_state(s):
                        # NOTE: We do not fix unreachable states, as we might
                        # not be able to compute the reduce actions. However,
                        # we should not clean edges not backedges as the state
                        # might become reachable later on, since states are
                        # shared if they have the same locations.
                        unreachable.append(s)
                        continue
                    assert self.states[s].is_inconsistent()
                    start_len = len(self.states)
                    if verbose:
                        count = count + 1
                        print("Fixing state {}\n".format(self.states[s].stable_str(self.states)))
                    try:
                        self.fix_inconsistent_state(s, verbose)
                    except Exception as exc:
                        self.debug_info = True
                        raise ValueError(
                            "Error while fixing conflict in state {}\n\n"
                            "In the following grammar productions:\n{}"
                            .format(self.states[s].stable_str(self.states),
                                    self.debug_context(s, "\n", "\t"))
                        ) from exc
                    new_inconsistent_states = [
                        s.index for s in self.states[start_len:]
                        if s.is_inconsistent()
                    ]
                    if verbose:
                        print("\tAdding {} states".format(len(self.states[start_len:])))
                        print("\tWith {} inconsistent states".format(len(new_inconsistent_states)))
                    todo.extend(new_inconsistent_states)

                # Check whether none of the previously inconsistent and
                # unreahable state became reachable. If so add it back to the
                # todo list.
                still_unreachable = []
                for s in unreachable:
                    if self.is_reachable_state(s):
                        todo.append(s)
                    else:
                        still_unreachable.append(s)
                unreachable = still_unreachable

        consume(visit_table(), progress)
        if verbose:
            print("\n".join([
                "\nGrammar is now consistent.",
                "\tNumber of States = {}",
                "\tNumber of inconsistencies solved = {}"]).format(
                    len(self.states), count))
        assert not self.is_inconsistent()
        self.assume_inconsistent = False

        if verbose:
            print("Fix Inconsistent Table Result:")
            self.debug_dump()

    def remove_all_unreachable_state(self, verbose: bool, progress: bool) -> None:
        self.states = [s for s in self.states if s is not None]
        self.rewrite_reordered_state_indexes()

    def lower_reduce_actions(self, verbose: bool, progress: bool) -> None:
        # Remove Reduce actions and replace them by the programmatic
        # equivalent.
        #
        # This transformation preserves the stack manipulations of the parse
        # table. It only changes it from being implicitly executed by the LR
        # parser, to being explicitly executed with actions.
        #
        # This transformation converts the hard-to-predict load of the shift
        # table into a branch prediction which is potentially easier to
        # predict.
        #
        # A side-effect of this transformation is that it removes the need for
        # replaying non-terminals, thus the backends could safely ignore the
        # ability of the shift function from handling non-terminals.
        if verbose or progress:
            print("Lower Reduce actions.")

        maybe_unreachable_set: OrderedSet[StateId] = OrderedSet()

        def transform() -> typing.Iterator[None]:
            for s in self.states:
                term, _ = next(iter(s.epsilon), (None, None))
                if self.term_is_shifted(term):
                    continue
                assert len(s.epsilon) == 1
                yield  # progress bar.
                reduce_state = s
                if verbose:
                    print("Inlining shift-operation for state {}".format(str(reduce_state)))

                # The reduced_aps should contain all reduced path of the single
                # Reduce action which is present on this state. However, as
                # state of the graph are shared, some reduced paths might follow
                # the same path and reach the same state.
                #
                # This code collect for each replayed path, the tops of the
                # stack on top of which these states are replayed.
                aps = APS.start(s.index)
                states_by_replay_term = collections.defaultdict(list)
                # print("Start:\n{}".format(aps.string(name="\titer_aps")))
                # print(s.stable_str(self.states))
                for reduced_aps in aps.shift_next(self):
                    # As long as we have elements to replay, we should only
                    # have a single path for each reduced path. If the next
                    # state contains an action, then we stop here.
                    iter_aps = reduced_aps
                    next_is_action = self.states[iter_aps.state].epsilon != []
                    has_replay = iter_aps.replay != []
                    assert next_is_action is False and has_replay is True
                    while (not next_is_action) and has_replay:
                        # print("Step {}:\n{}".format(len(iter_aps.history),
                        #                             iter_aps.string(name="\titer_aps")))
                        next_aps = list(iter_aps.shift_next(self))
                        if len(next_aps) == 0:
                            # Note, this might happen as we are adding
                            # lookahead tokens from any successor, we might not
                            # always have a way to replay all tokens, in such
                            # case an error should be produced, but in the mean
                            # time, let's use the shift function as usual.
                            break
                        assert len(next_aps) == 1
                        iter_aps = next_aps[0]
                        next_is_action = self.states[iter_aps.state].epsilon != []
                        has_replay = iter_aps.replay != []
                    # print("End at {}:\n{}".format(len(iter_aps.history),
                    #                               iter_aps.string(name="\titer_aps")))
                    replay_list = [e.src for e in iter_aps.shift]
                    assert len(replay_list) >= 2
                    replay_term = Replay(replay_list[1:])
                    states_by_replay_term[replay_term].append(replay_list[0])

                # Create FilterStates actions.
                filter_by_replay_term = {
                    replay_term: FilterStates(states)
                    for replay_term, states in states_by_replay_term.items()
                }

                # Convert the Reduce action to an Unwind action.
                reduce_term, _ = next(iter(s.epsilon))
                if isinstance(reduce_term, Reduce):
                    unwind_term: Action = reduce_term.unwind
                else:
                    assert isinstance(reduce_term, Seq)
                    assert isinstance(reduce_term.actions[-1], Reduce)
                    unwind_term = Seq(list(reduce_term.actions[:-1]) + [reduce_term.actions[-1].unwind])

                # Remove the old Reduce edge if still present.
                # print("Before:\n{}".format(reduce_state.stable_str(self.states)))
                self.remove_edge(reduce_state, reduce_term, maybe_unreachable_set)

                # Add Unwind action.
                # print("After:\n")
                locations = reduce_state.locations
                delayed: OrderedFrozenSet[DelayedAction] = OrderedFrozenSet(filter_by_replay_term.items())
                replay_size = 1  # Replay the unwound non-terminal
                is_new, filter_state = self.new_state(locations, delayed, replay_size)
                self.add_edge(reduce_state, unwind_term, filter_state.index)
                if not is_new:
                    # The destination state already exists. Assert that all
                    # outgoing edges are matching what we would have generated.
                    if len(filter_by_replay_term) == 1:
                        # There is only one predecessor, no need for a
                        # FilterState condition.
                        replay_term = next(iter(filter_by_replay_term))
                        assert replay_term in filter_state
                        continue

                    for replay_term, filter_term in filter_by_replay_term.items():
                        assert filter_term in filter_state
                        replay_state = self.states[filter_state[filter_term]]
                        assert replay_term in replay_state
                    continue

                if len(filter_by_replay_term) == 1:
                    replay_term = next(iter(filter_by_replay_term))
                    dest_idx = replay_term.replay_steps[-1]
                    # Do not add the FilterStates action, as there is only one.
                    # Add Replay actions from the filter_state to the destination.
                    self.add_edge(filter_state, replay_term, dest_idx)
                else:
                    for replay_term, filter_term in filter_by_replay_term.items():
                        dest_idx = replay_term.replay_steps[-1]
                        dest = self.states[dest_idx]

                        # Add FilterStates action from the filter_state to the replay_state.
                        locations = dest.locations
                        delayed = OrderedFrozenSet(itertools.chain(dest.delayed_actions, [replay_term]))
                        is_new, replay_state = self.new_state(locations, delayed, replay_size)
                        self.add_edge(filter_state, filter_term, replay_state.index)
                        assert (not is_new) == (replay_term in replay_state)

                        # Add Replay actions from the replay_state to the destination.
                        if is_new:
                            dest_idx = replay_term.replay_steps[-1]
                            self.add_edge(replay_state, replay_term, dest_idx)
                        # print(replay_state.stable_str(self.states))
                        assert not replay_state.is_inconsistent()

                # print(filter_state.stable_str(self.states))
                # print(reduce_state.stable_str(self.states))
                assert not reduce_state.is_inconsistent()
                assert not filter_state.is_inconsistent()

        consume(transform(), progress)

    def fold_replay_unwind(self, verbose: bool, progress: bool) -> None:
        """Convert Replay action falling into Unwind action to an Unwind action which
        replay less terms."""
        if verbose or progress:
            print("Fold Replay followed by Unwind actions.")

        maybe_unreachable_set: OrderedSet[StateId] = OrderedSet()

        def try_transform(s: StateAndTransitions) -> bool:
            if len(s.epsilon) != 1:
                return False
            replay_term, replay_dest_idx = next(iter(s.epsilon))
            if not isinstance(replay_term, Replay):
                return False
            replay_dest = self.states[replay_dest_idx]
            if len(replay_dest.epsilon) != 1:
                return False
            unwind_term, unwind_dest_idx = next(iter(replay_dest.epsilon))
            if not unwind_term.update_stack():
                return False
            stack_diff = unwind_term.update_stack_with()
            if not stack_diff.reduce_stack():
                return False
            if stack_diff.pop + stack_diff.replay <= 0:
                return False

            # Remove replayed terms from the Unwind action.
            replayed = replay_term.replay_steps
            unshifted = min(stack_diff.replay + min(s.arguments, stack_diff.pop), len(replayed))
            if unshifted < len(replayed):
                # We do not have all replayed terms as arguments, thus do not
                # consume arguments
                unshifted = min(stack_diff.replay, len(replayed))
            if unshifted == 0:
                return False
            new_unwind_term = unwind_term.unshift_action(unshifted)
            new_replay = new_unwind_term.update_stack_with().replay

            # Replace the replay_term and unwind_term by terms which are
            # avoiding extra replay actions.
            self.remove_edge(s, replay_term, maybe_unreachable_set)
            if len(replayed) == unshifted:
                # The Unwind action replay more terms than what we originally
                # had. The replay term is replaced by an Unwind edge instead.
                assert s.arguments >= -new_replay
                self.add_edge(s, new_unwind_term, unwind_dest_idx)
            else:
                # The Unwind action replay and pop less terms than what we
                # originally had. Thus the replay action is shortened and a new
                # state is created to accomodate the new Unwind action.
                assert unshifted >= 1
                new_replay_term = Replay(replayed[:-unshifted])
                implicit_replay_term = Replay(replayed[-unshifted:])
                locations = replay_dest.locations
                delayed: OrderedFrozenSet[DelayedAction]
                delayed = OrderedFrozenSet(
                    itertools.chain(replay_dest.delayed_actions, [implicit_replay_term]))
                is_new, unwind_state = self.new_state(locations, delayed)
                assert (not is_new) == (new_unwind_term in unwind_state)

                # Add new Replay and new Unwind actions.
                self.add_edge(s, new_replay_term, unwind_state.index)
                if is_new:
                    assert unwind_state.arguments >= -new_replay
                    self.add_edge(unwind_state, new_unwind_term, unwind_dest_idx)
                assert not unwind_state.is_inconsistent()
            assert not s.is_inconsistent()
            return True

        def transform() -> typing.Iterator[None]:
            for s in self.states:
                if try_transform(s):
                    yield  # progress bar

        consume(transform(), progress)
        self.remove_unreachable_states(maybe_unreachable_set)

    def fold_identical_endings(self, verbose: bool, progress: bool) -> None:
        # If 2 states have the same outgoing edges, then we can merge the 2
        # states into a single state, and rewrite all the backedges leading to
        # these states to be replaced by edges going to the reference state.
        if verbose or progress:
            print("Fold identical endings.")

        def rewrite_backedges(state_list: typing.List[StateAndTransitions],
                              state_map: typing.Dict[StateId, StateId],
                              backrefs: typing.Dict[StateId,
                                                    typing.List[typing.Tuple[StateId, Action, StateId]]],
                              maybe_unreachable: OrderedSet[StateId]) -> bool:
            all_backrefs = []
            new_backrefs = set()
            for s in state_list:
                all_backrefs.extend(backrefs[s.index])
            # All states have the same outgoing edges. Thus we replace all of
            # them by a single state. We do that by replacing edges of which
            # are targeting the state in the state_list by edges targetting the
            # ref state.
            ref = state_list.pop()
            tmp_state_map = default_fwd_dict(state_map)
            for s in state_list:
                tmp_state_map[s.index] = ref.index

            for ref_s, ref_t, _d in all_backrefs:
                new_backrefs.add((ref_s, ref_t.rewrite_state_indexes(tmp_state_map)))
            if len(all_backrefs) != len(new_backrefs):
                # Skip this rewrite if when rewritting we are going to cause
                # some aliasing to happen between actions which are going to
                # different states.
                return False

            replace_edges = [e for s in state_list for e in s.backedges]
            hit = False
            for edge in replace_edges:
                edge_term = edge.term
                assert edge_term is not None
                src = self.states[edge.src]
                old_dest = src[edge_term]
                # print("replace {} -- {} --> {}, by {} -- {} --> {}"
                #       .format(src.index, edge_term, src[edge_term], src.index, edge_term, ref.index))
                self.replace_edge(src, edge_term, ref.index, maybe_unreachable)
                state_map[old_dest] = ref.index
                hit = True
            return hit

        def rewrite_if_same_outedges(state_list: typing.List[StateAndTransitions]) -> bool:
            maybe_unreachable: OrderedSet[StateId] = OrderedSet()
            backrefs = collections.defaultdict(list)
            outedges = collections.defaultdict(list)
            for s in state_list:
                # Iterate first over actions, then over ordinary states.
                self.assert_state_invariants(s)
                outedges[tuple(s.edges())].append(s)
                if s.epsilon == []:
                    continue
                for t, d in s.edges():
                    if not isinstance(t, Action):
                        continue
                    for r in t.state_refs():
                        backrefs[r].append((s.index, t, d))
            hit = False
            state_map: typing.Dict[StateId, StateId] = default_id_dict()
            for same in outedges.values():
                if len(same) > 1:
                    hit = rewrite_backedges(same, state_map, backrefs, maybe_unreachable) or hit
            if hit:
                self.remove_unreachable_states(maybe_unreachable)
                self.rewrite_state_indexes(state_map)
                self.remove_all_unreachable_state(verbose, progress)
            return hit

        def visit_table() -> typing.Iterator[None]:
            hit = True
            while hit:
                yield  # progress bar.
                hit = rewrite_if_same_outedges(self.states)

        consume(visit_table(), progress)

    def group_epsilon_states(self, verbose: bool, progress: bool) -> None:
        def all_action_inedges(s: StateAndTransitions) -> bool:
            return all(isinstance(e.term, Action) for e in s.backedges)
        shift_states, action_states = split(self.states, lambda s: len(s.epsilon) == 0)
        from_act_action_states, from_shf_action_states = split(action_states, all_action_inedges)
        self.states = []
        self.states.extend(shift_states)
        self.states.extend(from_shf_action_states)
        self.states.extend(from_act_action_states)
        self.rewrite_reordered_state_indexes()

    def group_nonterminal_states(self, verbose: bool, progress: bool) -> None:
        # This function is used to reduce the range of FilterStates values,
        # such that the Rust compiler can compile FilterStates match statements
        # to a table-switch.
        freq_count = collections.Counter(nt for s in self.states for nt in s.nonterminals)
        freq_nt, _ = zip(*freq_count.most_common())

        def state_value(s: StateAndTransitions) -> float:
            value = 0.0
            if len(s.epsilon) != 0:
                return 4.0
            if len(s.nonterminals) == 0:
                return 2.0
            i = 1.0
            for nt in freq_nt:
                if nt in s:
                    value += i
                i /= 2.0
            return -value
        self.states.sort(key=state_value)
        self.rewrite_reordered_state_indexes()

    def count_shift_states(self) -> int:
        return sum(1 for s in self.states if s is not None and len(s.epsilon) == 0)

    def count_action_states(self) -> int:
        return sum(1 for s in self.states if s is not None and len(s.epsilon) > 0)

    def count_action_from_shift_states(self) -> int:
        def from_shift_states(s: StateAndTransitions) -> bool:
            return any(not isinstance(e.term, Action) for e in s.backedges)

        return sum(1 for s in self.states if len(s.epsilon) > 0 and from_shift_states(s))

    def prepare_debug_context(self) -> DebugInfo:
        """To better filter out the traversal of the grammar in debug context, we
        pre-compute for each state the maximal depth of each state within a
        production. Therefore, if visiting a state no increases the reducing
        depth beyind the ability to shrink the shift list to 0, then we can
        stop going deeper, as we entered a different production. """
        depths = collections.defaultdict(lambda: [])
        for s in self.states:
            if s is None or not s.epsilon:
                continue
            aps = APS.start(s.index)
            for aps_next in aps.shift_next(self):
                if not aps_next.reducing:
                    continue
                for i, edge in enumerate(aps_next.stack):
                    depths[edge.src].append(i + 1)
        return {s: max(ds) for s, ds in depths.items()}

    def debug_context(
            self,
            state: StateId,
            split_txt: str = "; ",
            prefix: str = ""
    ) -> str:
        """Reconstruct the grammar production by traversing the parse table."""
        if self.debug_info is False:
            return ""
        if self.debug_info is True:
            self.debug_info = self.prepare_debug_context()
        debug_info = typing.cast(typing.Dict[StateId, int], self.debug_info)

        record = []

        def visit(aps: APS) -> bool:
            # Stop after reducing once.
            if aps.history == []:
                return True
            last = aps.history[-1].term
            is_unwind = isinstance(last, Action) and last.update_stack()
            has_shift_loop = len(aps.shift) != 1 + len(set(zip(aps.shift, aps.shift[1:])))
            can_reduce_later = True
            try:
                can_reduce_later = debug_info[aps.shift[-1].src] >= len(aps.shift)
            except KeyError:
                can_reduce_later = False
            stop = is_unwind or has_shift_loop or not can_reduce_later
            # Record state which are reducing at most all the shifted states.
            save = stop and len(aps.shift) == 1
            save = save and is_unwind
            if save:
                assert isinstance(last, Action)
                save = last.update_stack_with().nt in self.states[aps.shift[0].src]
            if save:
                record.append(aps)
            return not stop

        self.aps_visitor(APS.start(state), visit)

        context: OrderedSet[str] = OrderedSet()
        for aps in record:
            assert aps.history != []
            action = aps.history[-1].term
            assert isinstance(action, Action)
            assert action.update_stack()
            stack_diff = action.update_stack_with()
            replay = stack_diff.replay
            before = [repr(e.term) for e in aps.stack[:-1]]
            after = [repr(e.term) for e in aps.history[:-1]]
            prod = before + ["\N{MIDDLE DOT}"] + after
            if replay < len(after) and replay > 0:
                del prod[-replay:]
                replay = 0
            if replay > len(after):
                replay += 1
            if replay > 0:
                prod = prod[:-replay] + ["[lookahead:"] + prod[-replay:] + ["]"]
            txt = "{}{} ::= {}".format(prefix, repr(stack_diff.nt), " ".join(prod))
            context.add(txt)

        if split_txt is None:
            return context
        return split_txt.join(txt for txt in sorted(context))
