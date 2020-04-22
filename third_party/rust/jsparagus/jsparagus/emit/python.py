"""Emit code and parser tables in Python."""

from __future__ import annotations

import io
import typing

from ..grammar import ErrorSymbol, Nt, Some
from ..actions import (Action, CheckNotOnNewLine, FilterFlag, FunCall,
                       Lookahead, OutputExpr, PopFlag, PushFlag, Reduce, Seq)
from ..runtime import ErrorToken, ErrorTokenClass
from ..ordered import OrderedSet
from ..lr0 import Term
from ..aps import StateId
from ..parse_table import ParseTable


def method_name_to_python(name: str) -> str:
    return name.replace(" ", "_")


def write_python_parse_table(out: io.TextIOBase, parse_table: ParseTable) -> None:
    # Disable MyPy type checking for everything in this module.
    out.write("# type: ignore\n\n")

    shift_count = parse_table.count_shift_states()
    action_count = parse_table.count_action_states()
    out.write("from jsparagus import runtime\n")
    if any(isinstance(key, Nt) for key in parse_table.nonterminals):
        out.write(
            "from jsparagus.runtime import (Nt, InitNt, End, ErrorToken, StateTermValue,\n"
            "                               ShiftError, ShiftAccept)\n")
    out.write("\n")

    methods: OrderedSet[FunCall] = OrderedSet()

    def write_action(act: Action, indent: str = "") -> typing.Tuple[str, bool]:
        assert not act.is_inconsistent()
        if isinstance(act, Reduce):
            out.write("{}replay = [StateTermValue(0, {}, value, False)]\n".format(indent, repr(act.nt)))
            if act.replay > 0:
                out.write("{}replay = replay + parser.stack[-{}:]\n".format(indent, act.replay))
            if act.replay + act.pop > 0:
                out.write("{}del parser.stack[-{}:]\n".format(indent, act.replay + act.pop))
            out.write("{}parser.shift_list(replay, lexer)\n".format(indent))
            return indent, False
        if isinstance(act, Lookahead):
            raise ValueError("Unexpected Lookahead action")
        if isinstance(act, CheckNotOnNewLine):
            out.write("{}if not parser.check_not_on_new_line(lexer, {}):\n".format(indent, -act.offset))
            out.write("{}    return\n".format(indent))
            return indent, True
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
            elif act.method == "accept":
                assert len(act.args) == 0
                out.write("{}raise ShiftAccept()\n".format(indent))
            else:
                methods.add(act)
                out.write("{}{} = parser.methods.{}({})\n".format(
                    indent, act.set_to, method_name_to_python(act.method),
                    ", ".join(map_with_offset(act.args))
                ))
            return indent, True
        if isinstance(act, Seq):
            res = True
            for a in act.actions:
                indent, fallthrough = write_action(a, indent)
            return indent, fallthrough
        raise ValueError("Unknown action type")

    # Write code correspond to each action which has to be performed.
    for i, state in enumerate(parse_table.states):
        assert i == state.index
        if state.epsilon == []:
            continue
        out.write("def state_{}_actions(parser, lexer):\n".format(i))
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
                if dest >= shift_count:
                    assert dest < shift_count + action_count
                    # This is a transition to an action.
                    out.write("{}state_{}_actions(parser, lexer)\n".format(indent, dest))
                else:
                    # This is a transition to a shift.
                    out.write("{}top = parser.stack.pop()\n".format(indent))
                    out.write("{}top = StateTermValue({}, top.term, top.value, top.new_line)\n"
                              .format(indent, dest))
                    out.write("{}parser.stack.append(top)\n".format(indent))
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
    for act in methods:
        assert isinstance(act, FunCall)
        args = ", ".join("x{}".format(i) for i in range(len(act.args)))
        name = method_name_to_python(act.method)
        out.write("    def {}(self, {}):\n".format(name, args))
        out.write("        return ({}, {})\n".format(repr(name), args))
    if not methods:
        out.write("    pass\n")
    out.write("\n")

    out.write("class Parser(runtime.Parser):\n")
    out.write("    def __init__(self, goal{}, builder=None):\n".format(default_goal))
    out.write("        if builder is None:\n")
    out.write("            builder = DefaultMethods()\n")
    out.write("        super().__init__(actions, error_codes, goal_nt_to_init_state[goal], builder)\n")
    out.write("\n")
