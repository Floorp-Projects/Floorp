from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

import typing

from .grammar import Element, InitNt, Nt
from . import types, grammar


class Action:
    __slots__ = ["read", "write", "_hash"]

    # Set of trait names which are consumed by this action.
    read: typing.List[str]

    # Set of trait names which are mutated by this action.
    write: typing.List[str]

    # Cached hash.
    _hash: typing.Optional[int]

    def __init__(self, read: typing.List[str], write: typing.List[str]) -> None:
        self.read = read
        self.write = write
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

    def update_stack(self) -> bool:
        """Change the parser stack, and resume at a different location. If this function
        is defined, then the function reduce_with should be implemented."""
        return False

    def reduce_with(self) -> Reduce:
        """Returns the Reduce action with which this action is reducing."""
        assert self.update_stack()
        raise TypeError("Action.reduce_with not implemented.")

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

    def __eq__(self, other: object) -> bool:
        if self.__class__ != other.__class__:
            return False
        assert isinstance(other, Action)
        if sorted(self.read) != sorted(other.read):
            return False
        if sorted(self.write) != sorted(other.write):
            return False
        for s in self.__slots__:
            if getattr(self, s) != getattr(other, s):
                return False
        return True

    def __hash__(self) -> int:
        if self._hash is not None:
            return self._hash

        def hashed_content() -> typing.Iterator[object]:
            yield self.__class__
            yield "rd"
            for alias in self.read:
                yield alias
            yield "wd"
            for alias in self.write:
                yield alias
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


class Reduce(Action):
    """Define a reduce operation which pops N elements of he stack and pushes one
    non-terminal. The replay attribute of a reduce action corresponds to the
    number of stack elements which would have to be popped and pushed again
    using the parser table after reducing this operation. """
    __slots__ = ['nt', 'replay', 'pop']

    nt: Nt
    pop: int
    replay: int

    def __init__(self, nt: Nt, pop: int, replay: int = 0) -> None:
        nt_name = nt.name
        if isinstance(nt_name, InitNt):
            name = "Start_" + str(nt_name.goal.name)
        else:
            name = nt_name
        super().__init__([], ["nt_" + name])
        self.nt = nt    # Non-terminal which is reduced
        self.pop = pop  # Number of stack elements which should be replayed.
        self.replay = replay  # List of terms to shift back

    def __str__(self) -> str:
        return "Reduce({}, {}, {})".format(self.nt, self.pop, self.replay)

    def update_stack(self) -> bool:
        return True

    def reduce_with(self) -> Reduce:
        return self

    def shifted_action(self, shifted_term: Element) -> Reduce:
        return Reduce(self.nt, self.pop, replay=self.replay + 1)


class Accept(Action):
    """This state terminate the parser by accepting the content consumed until
    now."""
    __slots__: typing.List[str] = []

    def __init__(self) -> None:
        super().__init__([], [])

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
        super().__init__([], [])
        self.terms = terms
        self.accept = accept

    def is_inconsistent(self) -> bool:
        # A lookahead restriction cannot be encoded in code, it has to be
        # solved using fix_with_lookahead.
        return True

    def is_condition(self) -> bool:
        return True

    def condition(self) -> Lookahead:
        return self

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
        super().__init__([], [])
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

    def shifted_action(self, shifted_term: Element) -> ShiftedAction:
        if isinstance(shifted_term, Nt):
            return True
        return CheckNotOnNewLine(self.offset - 1)

    def __str__(self) -> str:
        return "CheckNotOnNewLine({})".format(self.offset)


class FilterFlag(Action):
    """Define a filter which check for one value of the flag, and continue to the
    next state if the top of the flag stack matches the expected value."""
    __slots__ = ['flag', 'value']

    flag: str
    value: object

    def __init__(self, flag: str, value: object) -> None:
        super().__init__(["flag_" + flag], [])
        self.flag = flag
        self.value = value

    def is_condition(self) -> bool:
        return True

    def condition(self) -> FilterFlag:
        return self

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
        super().__init__([], ["flag_" + flag])
        self.flag = flag
        self.value = value

    def __str__(self) -> str:
        return "PushFlag({}, {})".format(self.flag, self.value)


class PopFlag(Action):
    """Define an action which pops a flag from the flag bit stack."""
    __slots__ = ['flag']

    flag: str

    def __init__(self, flag: str) -> None:
        super().__init__(["flag_" + flag], ["flag_" + flag])
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
            alias_read: typing.List[str] = [],
            alias_write: typing.List[str] = []
    ) -> None:
        super().__init__(alias_read, alias_write)
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
            self.trait, self.method, self.fallible, self.read, self.write,
            self.args, self.set_to, self.offset
        ])))

    def shifted_action(self, shifted_term: Element) -> FunCall:
        return FunCall(self.method,
                       self.args,
                       trait=self.trait,
                       fallible=self.fallible,
                       set_to=self.set_to,
                       offset=self.offset + 1,
                       alias_read=self.read,
                       alias_write=self.write)


class Seq(Action):
    """Aggregate multiple actions in one statement. Note, that the aggregated
    actions should not contain any condition or action which are mutating the
    state. Only the last action aggregated can update the parser stack"""
    __slots__ = ['actions']

    actions: typing.Tuple[Action, ...]

    def __init__(self, actions: typing.Sequence[Action]) -> None:
        read = [rd for a in actions for rd in a.read]
        write = [wr for a in actions for wr in a.write]
        super().__init__(read, write)
        self.actions = tuple(actions)   # Ordered list of actions to execute.
        assert all([not a.is_condition() for a in actions])
        assert all([not isinstance(a, Seq) for a in actions])
        assert all([not a.update_stack() for a in actions[:-1]])

    def __str__(self) -> str:
        return "{{ {} }}".format("; ".join(map(str, self.actions)))

    def __repr__(self) -> str:
        return "Seq({})".format(repr(self.actions))

    def is_condition(self) -> bool:
        return self.actions[0].is_condition()

    def condition(self) -> Action:
        return self.actions[0]

    def update_stack(self) -> bool:
        return self.actions[-1].update_stack()

    def reduce_with(self) -> Reduce:
        return self.actions[-1].reduce_with()

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
