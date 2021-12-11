"""Emit code and parser tables in Python."""

from __future__ import annotations

import io
import typing

from ..grammar import ErrorSymbol, Nt, Some
from ..actions import (Accept, Action, CheckNotOnNewLine, FilterFlag, FilterStates, FunCall,
                       Lookahead, OutputExpr, PopFlag, PushFlag, Reduce, Replay, Seq, Unwind)
from ..runtime import ErrorToken, ErrorTokenClass
from ..ordered import OrderedSet
from ..lr0 import Term
from ..parse_table import StateId, ParseTable


def method_name_to_python(name: str) -> str:
    return name.replace(" ", "_")


def write_python_parse_table(out: io.TextIOBase, parse_table: ParseTable) -> None:
    # Disable MyPy type checking for everything in this module.
    out.write("# type: ignore\n\n")

    out.write("from jsparagus import runtime\n")
    if any(isinstance(key, Nt) for key in parse_table.nonterminals):
        out.write(
            "from jsparagus.runtime import (Nt, InitNt, End, ErrorToken, StateTermValue,\n"
            "                               ShiftError, ShiftAccept)\n")
    out.write("\n")

    methods: OrderedSet[typing.Tuple[str, int]] = OrderedSet()

    def write_epsilon_transition(indent: str, dest_idx: StateId):
        dest = parse_table.states[dest_idx]
        if dest.epsilon != []:
            assert dest.index < len(parse_table.states)
            # This is a transition to an action.
            args = ""
            for i in range(dest.arguments):
                out.write("{}r{} = parser.replay.pop()\n".format(indent, i))
                args += ", r{}".format(i)
            out.write("{}state_{}_actions(parser, lexer{})\n".format(indent, dest.index, args))
        else:
            # This is a transition to a shift.
            assert dest.arguments == 0
            out.write("{}top = parser.stack.pop()\n".format(indent))
            out.write("{}top = StateTermValue({}, top.term, top.value, top.new_line)\n"
                      .format(indent, dest.index))
            out.write("{}parser.stack.append(top)\n".format(indent))

    def write_action(act: Action, indent: str = "") -> typing.Tuple[str, bool]:
        assert not act.is_inconsistent()
        if isinstance(act, Replay):
            for s in act.replay_steps:
                out.write("{}parser.replay_action({})\n".format(indent, s))
            return indent, True
        if isinstance(act, (Unwind, Reduce)):
            stack_diff = act.update_stack_with()
            replay = stack_diff.replay
            out.write("{}replay = []\n".format(indent))
            while replay > 0:
                replay -= 1
                out.write("{}replay.append(parser.stack.pop())\n".format(indent))
            out.write("{}replay.append(StateTermValue(0, {}, value, False))\n"
                      .format(indent, repr(stack_diff.nt)))
            if stack_diff.pop > 0:
                out.write("{}del parser.stack[-{}:]\n".format(indent, stack_diff.pop))
            out.write("{}parser.replay.extend(replay)\n".format(indent))
            return indent, act.follow_edge()
        if isinstance(act, Accept):
            out.write("{}raise ShiftAccept()\n".format(indent))
            return indent, False
        if isinstance(act, Lookahead):
            raise ValueError("Unexpected Lookahead action")
        if isinstance(act, CheckNotOnNewLine):
            out.write("{}if not parser.check_not_on_new_line(lexer, {}):\n".format(indent, -act.offset))
            out.write("{}    return\n".format(indent))
            return indent, True
        if isinstance(act, FilterStates):
            out.write("{}if parser.top_state() in [{}]:\n".format(indent, ", ".join(map(str, act.states))))
            return indent + "    ", True
        if isinstance(act, FilterFlag):
            out.write("{}if parser.flags[{}][-1] == {}:\n".format(indent, act.flag, act.value))
            return indent + "    ", True
        if isinstance(act, PushFlag):
            out.write("{}parser.flags[{}].append({})\n".format(indent, act.flag, act.value))
            return indent, True
        if isinstance(act, PopFlag):
            out.write("{}parser.flags[{}].pop()\n".format(indent, act.flag))
            return indent, True
        if isinstance(act, FunCall):
            enclosing_call_offset = act.offset
            if enclosing_call_offset < 0:
                # When replayed terms are given as function arguments, they are
                # not part of the stack. However, we cheat the system by
                # replaying all terms necessary to pop them uniformly. Thus, the
                # naming of variable for negative offsets will always match the
                # naming of when the offset is 0.
                enclosing_call_offset = 0

            def map_with_offset(args: typing.Iterable[OutputExpr]) -> typing.Iterator[str]:
                get_value = "parser.stack[{}].value"
                for a in args:
                    if isinstance(a, int):
                        yield get_value.format(-(a + enclosing_call_offset))
                    elif isinstance(a, str):
                        yield a
                    elif isinstance(a, Some):
                        # `typing.cast` because Some isn't generic, unfortunately.
                        yield next(map_with_offset([typing.cast(OutputExpr, a.inner)]))
                    elif a is None:
                        yield "None"
                    else:
                        raise ValueError(a)

            if act.method == "id":
                assert len(act.args) == 1
                out.write("{}{} = {}\n".format(indent, act.set_to, next(map_with_offset(act.args))))
            else:
                methods.add((act.method, len(act.args)))
                out.write("{}{} = parser.methods.{}({})\n".format(
                    indent, act.set_to, method_name_to_python(act.method),
                    ", ".join(map_with_offset(act.args))
                ))
            return indent, True
        if isinstance(act, Seq):
            for a in act.actions:
                indent, fallthrough = write_action(a, indent)
            return indent, fallthrough
        raise ValueError("Unknown action type")

    # Write code correspond to each action which has to be performed.
    for i, state in enumerate(parse_table.states):
        assert i == state.index
        if state.epsilon == []:
            continue
        args = []
        for j in range(state.arguments):
            args.append("a{}".format(j))
        out.write("def state_{}_actions(parser, lexer{}):\n".format(
            i, "".join(map(lambda s: ", " + s, args))))
        if state.arguments > 0:
            out.write("    parser.replay.extend([{}])\n".format(", ".join(reversed(args))))
        term, dest = next(iter(state.epsilon))
        if term.update_stack():
            # If we Unwind, make sure all elements are replayed on the stack before starting.
            out.write("    # {}\n".format(term))
            stack_diff = term.update_stack_with()
            replay = stack_diff.replay
            if stack_diff.pop + replay >= 0:
                while replay < 0:
                    replay += 1
                    out.write("    parser.stack.append(parser.replay.pop())\n")
        out.write("{}\n".format(parse_table.debug_context(i, "\n", "    # ")))
        out.write("    value = None\n")
        for action, dest in state.edges():
            assert isinstance(action, Action)
            try:
                indent, fallthrough = write_action(action, "    ")
            except Exception:
                print("Error while writing code for {}\n\n".format(state))
                parse_table.debug_info = True
                print(parse_table.debug_context(state.index, "\n", "# "))
                raise
            if fallthrough:
                write_epsilon_transition(indent, dest)
            out.write("{}return\n".format(indent))
        out.write("\n")

    out.write("actions = [\n")
    for i, state in enumerate(parse_table.states):
        assert i == state.index
        out.write("    # {}.\n{}\n".format(i, parse_table.debug_context(i, "\n", "    # ")))
        if state.epsilon == []:
            row: typing.Dict[typing.Union[Term, ErrorTokenClass], StateId]
            row = {term: dest for term, dest in state.edges()}
            for err, dest in state.errors.items():
                del row[err]
                row[ErrorToken] = dest
            out.write("    " + repr(row) + ",\n")
        else:
            out.write("    state_{}_actions,\n".format(i))
        out.write("\n")
    out.write("]\n\n")

    out.write("error_codes = [\n")

    def repr_code(symb: typing.Optional[ErrorSymbol]) -> str:
        if isinstance(symb, ErrorSymbol):
            return repr(symb.error_code)
        return repr(symb)

    SLICE_LEN = 16
    for i in range(0, len(parse_table.states), SLICE_LEN):
        states_slice = parse_table.states[i:i + SLICE_LEN]
        out.write("    {}\n".format(
            " ".join(repr_code(state.get_error_symbol()) + ","
                     for state in states_slice)))
    out.write("]\n\n")

    out.write("goal_nt_to_init_state = {}\n\n".format(
        repr({nt.name: goal for nt, goal in parse_table.named_goals})
    ))

    if len(parse_table.named_goals) == 1:
        init_nt = parse_table.named_goals[0][0]
        default_goal = '=' + repr(init_nt.name)
    else:
        default_goal = ''

    # Class used to provide default methods when not defined by the caller.
    out.write("class DefaultMethods:\n")
    for method, arglen in methods:
        act_args = ", ".join("x{}".format(i) for i in range(arglen))
        name = method_name_to_python(method)
        out.write("    def {}(self, {}):\n".format(name, act_args))
        out.write("        return ({}, {})\n".format(repr(name), act_args))
    if not methods:
        out.write("    pass\n")
    out.write("\n")

    out.write("class Parser(runtime.Parser):\n")
    out.write("    def __init__(self, goal{}, builder=None):\n".format(default_goal))
    out.write("        if builder is None:\n")
    out.write("            builder = DefaultMethods()\n")
    out.write("        super().__init__(actions, error_codes, goal_nt_to_init_state[goal], builder)\n")
    out.write("\n")
