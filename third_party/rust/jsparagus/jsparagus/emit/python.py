"""Emit code and parser tables in Python."""

from ..grammar import Some, Nt, ErrorSymbol
from ..actions import (Action, Reduce, Lookahead, CheckNotOnNewLine, FilterFlag, PushFlag, PopFlag,
                       FunCall, Seq)
from ..runtime import ErrorToken
from ..ordered import OrderedSet


def write_python_parse_table(out, parse_table):
    # Disable MyPy type checking for everything in this module.
    out.write("# type: ignore\n\n")

    out.write("from jsparagus import runtime\n")
    if any(isinstance(key, Nt) for key in parse_table.nonterminals):
        out.write(
            "from jsparagus.runtime import (Nt, InitNt, End, ErrorToken, StateTermValue,\n"
            "                               ShiftError, ShiftAccept)\n")
    out.write("\n")

    methods = OrderedSet()

    def write_action(act, indent=""):
        assert isinstance(act, Action)
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
            def map_with_offset(args):
                get_value = "parser.stack[{}].value"
                for a in args:
                    if isinstance(a, int):
                        yield get_value.format(-(a + act.offset))
                    elif isinstance(a, str):
                        yield a
                    elif isinstance(a, Some):
                        yield next(map_with_offset([a.inner]))
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
                    indent, act.set_to, act.method,
                    ", ".join(map_with_offset(act.args))
                ))
            return indent, True
        if isinstance(act, Seq):
            res = True
            for a in act.actions:
                indent, res = write_action(a, indent)
            return indent, res
        raise ValueError("Unknown action type")

    # Write code correspond to each action which has to be performed.
    for i, state in enumerate(parse_table.states):
        assert i == state.index
        if state.epsilon == []:
            continue
        out.write("def state_{}_actions(parser, lexer):\n".format(i))
        out.write("{}\n".format(parse_table.debug_context(i, "\n", "    # ")))
        out.write("    value = None\n")
        for term, dest in state.edges():
            try:
                indent, res = write_action(term, "    ")
            except Exception:
                print("Error while writing code for {}\n\n".format(state))
                parse_table.debug_info = True
                print(parse_table.debug_context(state.index, "\n", "# "))
                raise
            if res:
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
            row = {term: dest for term, dest in state.edges()}
            for err, dest in state.errors.items():
                del row[err]
                row[ErrorToken()] = dest
            out.write("    " + repr(row) + ",\n")
        else:
            out.write("    state_{}_actions,\n".format(i))
        out.write("\n")
    out.write("]\n\n")

    out.write("error_codes = [\n")

    def repr_code(symb):
        if isinstance(symb, ErrorSymbol):
            return repr(symb.error_code)
        return repr(symb)
    SLICE_LEN = 16
    for i in range(0, len(parse_table.states), SLICE_LEN):
        states_slice = parse_table.states[i:i + SLICE_LEN]
        out.write("    {}\n".format(
            " ".join(repr_code(state.get_error_symbol()) + "," for state in states_slice)))
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
        out.write("    def {}(self, {}):\n".format(act.method, args))
        out.write("        return ({}, {})\n".format(repr(act.method), args))
    if not methods:
        out.write("    pass\n")
    out.write("\n")

    out.write("class Parser(runtime.Parser):\n")
    out.write("    def __init__(self, goal{}, builder=None):\n".format(default_goal))
    out.write("        if builder is None:\n")
    out.write("            builder = DefaultMethods()\n")
    out.write("        super().__init__(actions, error_codes, goal_nt_to_init_state[goal], builder)\n")
    out.write("\n")
