from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

import typing
import dataclasses

from .ordered import OrderedFrozenSet
from .grammar import Element, ErrorSymbol, InitNt, Nt
from . import types, grammar

# Avoid circular reference between this module and parse_table.py
if typing.TYPE_CHECKING:
    from .parse_table import StateId


@dataclasses.dataclass(frozen=True)
class StackDiff:
    """StackDiff represent stack mutations which have to be performed when executing an action.
    """
    __slots__ = ['pop', 'nt', 'replay']

    # Example: We have shifted `b * c X Y`. We want to reduce `b * c` to Mul.
    #
    # In the initial LR0 pass over the grammar, this produces a Reduce edge.
    #
    # action         pop          replay
    # -------------- ------       ---------
    # Reduce         3 (`b * c`)  2 (`X Y`)
    #   The parser moves `X Y` to the replay queue, pops `b * c`, creates the
    #   new `Mul` nonterminal, consults the stack and parse table to determine
    #   the new state id, then replays the new nonterminal. Reduce leaves `X Y`
    #   on the runtime replay queue. It's the runtime's responsibility to
    #   notice that they are there and replay them.
    #
    # Later, the Reduce edge might be lowered into an [Unwind; FilterState;
    # Replay] sequence, which encode both the Reduce action, and the expected
    # behavior of the runtime.
    #
    # action         pop          replay
    # -------------- ------       ---------
    # Unwind         3            2
    #   The parser moves `X Y` to the replay queue, pops `b * c`, creates the
    #   new `Mul` nonterminal, and inserts it at the front of the replay queue.
    #
    # FilterState    ---          ---
    #   Determines the new state id, if it's context-dependent.
    #   This doesn't touch the stack, so no StackDiff.
    #
    # Replay         0            -3
    #   Shift the three elements we left on the replay queue back to the stack:
    #   `(b*c) X Y`.

    # Number of elements to be popped from the stack, this is used when
    # reducing the stack with a non-terminal.
    #
    # This number is always positive or zero.
    pop: int

    # When reducing, a non-terminal is pushed after removing all replayed and
    # popped elements. If not None, this is the non-terminal which is produced
    # by reducing the action.
    nt: typing.Union[Nt, ErrorSymbol, None]

    # Number of terms this action moves from the stack to the runtime replay
    # queue (not counting `self.nt`), or from the replay queue to the stack if
    # negative.
    #
    # When executing actions, some lookahead might have been used to make the
    # parse table consistent. Replayed terms are popped before popping any
    # elements from the stack, and they are added in reversed order in the
    # replay list, such that they would be shifted after shifting the `reduced`
    # non-terminal.
    #
    # This number might also be negative, in which case some lookahead terms
    # are expected to exists in the replay list, and they are shifted back.
    # This must happen only follow_edge is True.
    replay: int

    def reduce_stack(self) -> bool:
        """Returns whether the action is reducing the stack by replacing popped
        elements by a non-terminal. Note, this test is simpler than checking
        for instances, as Reduce / Unwind might either be present, or present
        as part of the last element of a Seq action. """
        return self.nt is not None


class Action:
    __slots__ = ["_hash"]

    # Cached hash.
    _hash: typing.Optional[int]

    def __init__(self) -> None:
        self._hash = None

    def is_inconsistent(self) -> bool:
        """Returns True if this action is inconsistent. An action can be
        inconsistent if the parameters it is given cannot be evaluated given
        its current location in the parse table. Such as CheckNotOnNewLine.
        """
        return False

    def is_condition(self) -> bool:
        "Unordered condition, which accept or not to reach the next state."
        return False

    def condition(self) -> Action:
        "Return the conditional action."
        raise TypeError("Action.condition not implemented")

    def check_same_variable(self, other: Action) -> bool:
        "Return whether both conditionals are checking the same variable."
        assert self.is_condition()
        raise TypeError("Action.check_same_variable not implemented")

    def check_different_values(self, other: Action) -> bool:
        "Return whether these 2 conditions are mutually exclusive."
        assert self.is_condition()
        raise TypeError("Action.check_different_values not implemented")

    def follow_edge(self) -> bool:
        """Whether the execution of this action resume following the epsilon transition
        (True) or if it breaks the graph epsilon transition (False) and returns
        at a different location, defined by the top of the stack."""
        return True

    def update_stack(self) -> bool:
        """Whether the execution of this action changes the parser stack."""
        return False

    def update_stack_with(self) -> StackDiff:
        """Returns a StackDiff which represents the mutation to be applied to the
        parser stack."""
        assert self.update_stack()
        raise TypeError("Action.update_stack_with not implemented")

    def unshift_action(self, num: int) -> Action:
        """When manipulating stack operation, we have the option to unshift some
        replayed token which were shifted to disambiguate the grammar. However,
        they might no longer be needed in some cases."""
        raise TypeError("{} cannot be unshifted".format(self.__class__.__name__))

    def shifted_action(self, shifted_term: Element) -> ShiftedAction:
        """Transpose this action with shifting the given terminal or Nt.

        That is, the sequence of:
        - performing the action `self`, then
        - shifting `shifted_term`
        has the same effect as:
        - shifting `shifted_term`, then
        - performing the action `self.shifted_action(shifted_term)`.

        If the resulting shifted action would be a no-op, instead return True.

        If this is a conditional action and `shifted_term` indicates that the
        condition wasn't met, return False.
        """
        return self

    def contains_accept(self) -> bool:
        "Returns whether the current action stops the parser."
        return False

    def rewrite_state_indexes(self, state_map: typing.Dict[StateId, StateId]) -> Action:
        """If the action contains any state index, use the map to map the old index to
        the new indexes"""
        return self

    def fold_by_destination(self, actions: typing.List[Action]) -> typing.List[Action]:
        """If after rewriting state indexes, multiple condition are reaching the same
        destination state, we attempt to fold them by destination. Not
        implementing this function can lead to the introduction of inconsistent
        states, as the conditions might be redundant. """

        # By default do nothing.
        return actions

    def state_refs(self) -> typing.List[StateId]:
        """List of states which are referenced by this action."""
        # By default do nothing.
        return []

    def __eq__(self, other: object) -> bool:
        if self.__class__ != other.__class__:
            return False
        assert isinstance(other, Action)
        for s in self.__slots__:
            if getattr(self, s) != getattr(other, s):
                return False
        return True

    def __hash__(self) -> int:
        if self._hash is not None:
            return self._hash

        def hashed_content() -> typing.Iterator[object]:
            yield self.__class__
            for s in self.__slots__:
                yield repr(getattr(self, s))

        self._hash = hash(tuple(hashed_content()))
        return self._hash

    def __lt__(self, other: Action) -> bool:
        return hash(self) < hash(other)

    def __repr__(self) -> str:
        return str(self)

    def stable_str(self, states: typing.Any) -> str:
        return str(self)


ShiftedAction = typing.Union[Action, bool]


class Replay(Action):
    """Replay a term which was previously saved by the Unwind function. Note that
    this does not Shift a term given as argument as the replay action should
    always be garanteed and that we want to maximize the sharing of code when
    possible."""
    __slots__ = ['replay_steps']

    replay_steps: typing.Tuple[StateId, ...]

    def __init__(self, replay_steps: typing.Iterable[StateId]):
        super().__init__()
        self.replay_steps = tuple(replay_steps)

    def update_stack(self) -> bool:
        return True

    def update_stack_with(self) -> StackDiff:
        return StackDiff(0, None, -len(self.replay_steps))

    def rewrite_state_indexes(self, state_map: typing.Dict[StateId, StateId]) -> Replay:
        return Replay(map(lambda s: state_map[s], self.replay_steps))

    def state_refs(self) -> typing.List[StateId]:
        return list(self.replay_steps)

    def __str__(self) -> str:
        return "Replay({})".format(str(self.replay_steps))


class Unwind(Action):
    """Define an unwind operation which pops N elements of the stack and pushes one
    non-terminal. The replay argument of an unwind action corresponds to the
    number of stack elements which would have to be popped and pushed again
    using the parser table after executing this operation."""
    __slots__ = ['nt', 'replay', 'pop']

    nt: Nt
    pop: int
    replay: int

    def __init__(self, nt: Nt, pop: int, replay: int = 0) -> None:
        super().__init__()
        self.nt = nt    # Non-terminal which is reduced
        self.pop = pop  # Number of stack elements which should be replayed.
        self.replay = replay  # List of terms to shift back

    def __str__(self) -> str:
        return "Unwind({}, {}, {})".format(self.nt, self.pop, self.replay)

    def update_stack(self) -> bool:
        return True

    def update_stack_with(self) -> StackDiff:
        return StackDiff(self.pop, self.nt, self.replay)

    def unshift_action(self, num: int) -> Unwind:
        return Unwind(self.nt, self.pop, replay=self.replay - num)

    def shifted_action(self, shifted_term: Element) -> Unwind:
        return Unwind(self.nt, self.pop, replay=self.replay + 1)


class Reduce(Action):
    """Prevent the fall-through to the epsilon transition and returns to the shift
    table execution to resume shifting or replaying terms."""
    __slots__ = ['unwind']

    unwind: Unwind

    def __init__(self, unwind: Unwind) -> None:
        nt_name = unwind.nt.name
        if isinstance(nt_name, InitNt):
            name = "Start_" + str(nt_name.goal.name)
        else:
            name = nt_name
        super().__init__()
        self.unwind = unwind

    def __str__(self) -> str:
        return "Reduce({})".format(str(self.unwind))

    def follow_edge(self) -> bool:
        return False

    def update_stack(self) -> bool:
        return self.unwind.update_stack()

    def update_stack_with(self) -> StackDiff:
        return self.unwind.update_stack_with()

    def unshift_action(self, num: int) -> Reduce:
        unwind = self.unwind.unshift_action(num)
        return Reduce(unwind)

    def shifted_action(self, shifted_term: Element) -> Reduce:
        unwind = self.unwind.shifted_action(shifted_term)
        return Reduce(unwind)


class Accept(Action):
    """This state terminate the parser by accepting the content consumed until
    now."""
    __slots__: typing.List[str] = []

    def __init__(self) -> None:
        super().__init__()

    def __str__(self) -> str:
        return "Accept()"

    def contains_accept(self) -> bool:
        "Returns whether the current action stops the parser."
        return True

    def shifted_action(self, shifted_term: Element) -> Accept:
        return Accept()


class Lookahead(Action):
    """Define a Lookahead assertion which is meant to either accept or reject
    sequences of terminal/non-terminals sequences."""
    __slots__ = ['terms', 'accept']

    terms: typing.FrozenSet[str]
    accept: bool

    def __init__(self, terms: typing.FrozenSet[str], accept: bool):
        super().__init__()
        self.terms = terms
        self.accept = accept

    def is_inconsistent(self) -> bool:
        # A lookahead restriction cannot be encoded in code, it has to be
        # solved using fix_with_lookahead, which encodes the lookahead
        # resolution in the generated parse table.
        return True

    def is_condition(self) -> bool:
        return True

    def condition(self) -> Lookahead:
        return self

    def check_same_variable(self, other: Action) -> bool:
        raise TypeError("Lookahead.check_same_variables: Lookahead are always inconsistent")

    def check_different_values(self, other: Action) -> bool:
        raise TypeError("Lookahead.check_different_values: Lookahead are always inconsistent")

    def __str__(self) -> str:
        return "Lookahead({}, {})".format(self.terms, self.accept)

    def shifted_action(self, shifted_term: Element) -> ShiftedAction:
        if isinstance(shifted_term, Nt):
            return True
        if shifted_term in self.terms:
            return self.accept
        return not self.accept


class CheckNotOnNewLine(Action):
    """Check whether the terminal at the given stack offset is on a new line or
    not. If not this would produce an Error, otherwise this rule would be
    shifted."""
    __slots__ = ['offset']

    offset: int

    def __init__(self, offset: int = 0) -> None:
        # assert offset >= -1 and "Smaller offsets are not supported on all backends."
        super().__init__()
        self.offset = offset

    def is_inconsistent(self) -> bool:
        # We can only look at stacked terminals. Having an offset of 0 implies
        # that we are looking for the next terminal, which is not yet shifted.
        # Therefore this action is inconsistent as long as the terminal is not
        # on the stack.
        return self.offset >= 0

    def is_condition(self) -> bool:
        return True

    def condition(self) -> CheckNotOnNewLine:
        return self

    def check_same_variable(self, other: Action) -> bool:
        return isinstance(other, CheckNotOnNewLine) and self.offset == other.offset

    def check_different_values(self, other: Action) -> bool:
        return False

    def shifted_action(self, shifted_term: Element) -> ShiftedAction:
        if isinstance(shifted_term, Nt):
            return True
        return CheckNotOnNewLine(self.offset - 1)

    def __str__(self) -> str:
        return "CheckNotOnNewLine({})".format(self.offset)


class FilterStates(Action):
    """Check whether the stack at a given depth match the state value, if so
    transition to the destination, otherwise check other states."""
    __slots__ = ['states']

    states: OrderedFrozenSet[StateId]

    def __init__(self, states: typing.Iterable[StateId]):
        super().__init__()
        # Set of states which can follow this transition.
        self.states = OrderedFrozenSet(sorted(states))

    def is_condition(self) -> bool:
        return True

    def condition(self) -> FilterStates:
        return self

    def check_same_variable(self, other: Action) -> bool:
        return isinstance(other, FilterStates)

    def check_different_values(self, other: Action) -> bool:
        assert isinstance(other, FilterStates)
        return self.states.is_disjoint(other.states)

    def rewrite_state_indexes(self, state_map: typing.Dict[StateId, StateId]) -> FilterStates:
        states = list(state_map[s] for s in self.states)
        return FilterStates(states)

    def fold_by_destination(self, actions: typing.List[Action]) -> typing.List[Action]:
        states: typing.List[StateId] = []
        for a in actions:
            if not isinstance(a, FilterStates):
                # Do nothing in case the state is inconsistent.
                return actions
            states.extend(a.states)
        return [FilterStates(states)]

    def state_refs(self) -> typing.List[StateId]:
        return list(self.states)

    def __str__(self) -> str:
        return "FilterStates({})".format(self.states)


class FilterFlag(Action):
    """Define a filter which check for one value of the flag, and continue to the
    next state if the top of the flag stack matches the expected value."""
    __slots__ = ['flag', 'value']

    flag: str
    value: object

    def __init__(self, flag: str, value: object) -> None:
        super().__init__()
        self.flag = flag
        self.value = value

    def is_condition(self) -> bool:
        return True

    def condition(self) -> FilterFlag:
        return self

    def check_same_variable(self, other: Action) -> bool:
        return isinstance(other, FilterFlag) and self.flag == other.flag

    def check_different_values(self, other: Action) -> bool:
        assert isinstance(other, FilterFlag)
        return self.value != other.value

    def __str__(self) -> str:
        return "FilterFlag({}, {})".format(self.flag, self.value)


class PushFlag(Action):
    """Define an action which pushes a value on a stack dedicated to the flag. This
    other stack correspond to another parse stack which live next to the
    default state machine and is popped by PopFlag, as-if this was another
    reduce action. This is particularly useful to raise the parse table from a
    LR(0) to an LR(k) without needing as much state duplications."""
    __slots__ = ['flag', 'value']

    flag: str
    value: object

    def __init__(self, flag: str, value: object) -> None:
        super().__init__()
        self.flag = flag
        self.value = value

    def __str__(self) -> str:
        return "PushFlag({}, {})".format(self.flag, self.value)


class PopFlag(Action):
    """Define an action which pops a flag from the flag bit stack."""
    __slots__ = ['flag']

    flag: str

    def __init__(self, flag: str) -> None:
        super().__init__()
        self.flag = flag

    def __str__(self) -> str:
        return "PopFlag({})".format(self.flag)


# OutputExpr: An expression mini-language that compiles very directly to code
# in the output language (Rust or Python). An OutputExpr is one of:
#
# str - an identifier in the generated code
# int - an index into the runtime stack
# None or Some(FunCallArg) - an optional value
#
OutputExpr = typing.Union[str, int, None, grammar.Some]


class FunCall(Action):
    """Define a call method operation which reads N elements of he stack and
    pushpathne non-terminal. The replay attribute of a reduce action correspond
    to the number of stack elements which would have to be popped and pushed
    again using the parser table after reducing this operation. """
    __slots__ = ['trait', 'method', 'offset', 'args', 'fallible', 'set_to']

    trait: types.Type
    method: str
    offset: int
    args: typing.Tuple[OutputExpr, ...]
    fallible: bool
    set_to: str

    def __init__(
            self,
            method: str,
            args: typing.Tuple[OutputExpr, ...],
            trait: types.Type = types.Type("AstBuilder"),
            fallible: bool = False,
            set_to: str = "val",
            offset: int = 0,
    ) -> None:
        super().__init__()
        self.trait = trait        # Trait on which this method is implemented.
        self.method = method      # Method and argument to be read for calling it.
        self.fallible = fallible  # Whether the function call can fail.
        self.offset = offset      # Offset to add to each argument offset.
        self.args = args          # Tuple of arguments offsets.
        self.set_to = set_to      # Temporary variable name to set with the result.

    def __str__(self) -> str:
        return "{} = {}::{}({}){} [off: {}]".format(
            self.set_to, self.trait, self.method,
            ", ".join(map(str, self.args)),
            self.fallible and '?' or '',
            self.offset)

    def __repr__(self) -> str:
        return "FunCall({})".format(', '.join(map(repr, [
            self.trait, self.method, self.fallible,
            self.args, self.set_to, self.offset
        ])))

    def unshift_action(self, num: int) -> FunCall:
        return FunCall(self.method, self.args,
                       trait=self.trait,
                       fallible=self.fallible,
                       set_to=self.set_to,
                       offset=self.offset - num)

    def shifted_action(self, shifted_term: Element) -> FunCall:
        return FunCall(self.method,
                       self.args,
                       trait=self.trait,
                       fallible=self.fallible,
                       set_to=self.set_to,
                       offset=self.offset + 1)


class Seq(Action):
    """Aggregate multiple actions in one statement. Note, that the aggregated
    actions should not contain any condition or action which are mutating the
    state. Only the last action aggregated can update the parser stack"""
    __slots__ = ['actions']

    actions: typing.Tuple[Action, ...]

    def __init__(self, actions: typing.Sequence[Action]) -> None:
        super().__init__()
        self.actions = tuple(actions)   # Ordered list of actions to execute.
        assert all([not a.is_condition() for a in actions])
        assert all([not isinstance(a, Seq) for a in actions])
        assert all([a.follow_edge() for a in actions[:-1]])
        assert all([not a.update_stack() for a in actions[:-1]])

    def __str__(self) -> str:
        return "{{ {} }}".format("; ".join(map(str, self.actions)))

    def __repr__(self) -> str:
        return "Seq({})".format(repr(self.actions))

    def follow_edge(self) -> bool:
        return self.actions[-1].follow_edge()

    def update_stack(self) -> bool:
        return self.actions[-1].update_stack()

    def update_stack_with(self) -> StackDiff:
        return self.actions[-1].update_stack_with()

    def unshift_action(self, num: int) -> Seq:
        actions = list(map(lambda a: a.unshift_action(num), self.actions))
        return Seq(actions)

    def shifted_action(self, shift: Element) -> ShiftedAction:
        actions: typing.List[Action] = []
        for a in self.actions:
            b = a.shifted_action(shift)
            if isinstance(b, bool):
                if b is False:
                    return False
            else:
                actions.append(b)
        return Seq(actions)

    def contains_accept(self) -> bool:
        return any(a.contains_accept() for a in self.actions)

    def rewrite_state_indexes(self, state_map: typing.Dict[StateId, StateId]) -> Seq:
        actions = list(map(lambda a: a.rewrite_state_indexes(state_map), self.actions))
        return Seq(actions)

    def state_refs(self) -> typing.List[StateId]:
        return [s for a in self.actions for s in a.state_refs()]
