"""Emit code and parser tables in Python."""

from ..grammar import InitNt, CallMethod, Some, is_concrete_element, Nt
from ..runtime import SPECIAL_CASE_TAG


def write_python_parser(out, parser_states):
    grammar = parser_states.grammar
    states = parser_states.states
    prods = parser_states.prods
    init_state_map = parser_states.init_state_map

    out.write("from jsparagus import runtime\n")
    if any(isinstance(key, Nt) for key in grammar.nonterminals):
        out.write("from jsparagus.runtime import Nt, ErrorToken\n")
    out.write("\n")

    special_case_cache = {}
    special_cases = []
    def render_action(action):
        if isinstance(action, tuple):
            if action not in special_case_cache:
                if action[0] == 'IfSameLine':
                    special_case_cache[action] = len(special_cases)
                    special_cases.append(
                        "lambda lexer, t: {} if lexer.saw_line_terminator() else {}"
                        .format(action[2], action[1]))
                else:
                    raise ValueError("unrecognized kind of special case: " + repr(action))
            index = special_case_cache[action]
            return SPECIAL_CASE_TAG + index
        else:
            assert isinstance(action, int)
            return action

    out.write("actions = [\n")
    for i, state in enumerate(states):
        out.write("    # {}. {}\n".format(i, state.traceback() or "<empty>"))
        # for item in state._lr_items:
        #     out.write("    #       {}\n".format(grammar.lr_item_to_str(prods, item)))
        out.write("    {"
                  + ", ".join("{!r}: {!r}".format(t, render_action(action))
                              for t, action in state.action_row.items())
                  + "},\n")
        out.write("\n")
    out.write("]\n\n")

    out.write("ctns = [\n")
    for state in states:
        row = {
            nt.pretty(): state_id
            for nt, state_id in state.ctn_row.items()
        }
        out.write("    " + repr(row) + ",\n")
    out.write("]\n\n")

    out.write("special_cases = [\n")
    for case in special_cases:
        out.write("    {},\n".format(case))
    out.write("]\n\n")

    out.write("error_codes = [\n")
    SLICE_LEN = 16
    for i in range(0, len(states), SLICE_LEN):
        slice = states[i:i + SLICE_LEN]
        out.write("    {}\n".format(
            " ".join(repr(e.error_code) + "," for e in slice)))
    out.write("]\n\n")

    def compile_reduce_expr(expr):
        """Compile a reduce expression to Python"""
        if isinstance(expr, CallMethod):
            method_name = expr.method.replace(" ", "_P")
            return "builder.{}({})".format(method_name, ', '.join(map(compile_reduce_expr, expr.args)))
        elif isinstance(expr, Some):
            return compile_reduce_expr(expr.inner)
        elif expr is None:
            return "None"
        else:
            # can't be 'accept' because we filter out InitNt productions
            assert isinstance(expr, int)
            return "x{}".format(expr)

    out.write("reductions = [\n")
    for prod_index, prod in enumerate(prods):
        if isinstance(prod.nt.name, InitNt):
            continue
        nparams = sum(1 for e in prod.rhs if is_concrete_element(e))
        names = ["x" + str(i) for i in range(nparams)]
        fn = ("lambda builder, "
              + ", ".join(names)
              + ": " + compile_reduce_expr(prod.reducer))
        out.write("    # {}. {}\n".format(
            prod_index,
            grammar.production_to_str(prod.nt, prod.rhs, prod.reducer)))
        out.write("    ({!r}, {!r}, {}),\n".format(prod.nt.pretty(), len(names), fn))
    out.write("]\n\n\n")  # two blank lines before class.

    out.write("class DefaultBuilder:\n")
    for tag, method_type in grammar.methods.items():
        method_name = tag.replace(' ', '_P')
        args = ", ".join("x{}".format(i)
                         for i in range(len(method_type.argument_types)))
        out.write("    def {}(self, {}): return ({!r}, {})\n"
                  .format(method_name, args, tag, args))
    out.write("\n\n")

    out.write("goal_nt_to_init_state = {\n")
    for init_nt, index in init_state_map.items():
        out.write("    {!r}: {!r},\n".format(init_nt.name, index))
    out.write("}\n\n")

    if len(init_state_map) == 1:
        init_nt = next(iter(init_state_map.keys()))
        default_goal = '=' + repr(init_nt.name)
    else:
        default_goal = ''
    out.write("class Parser(runtime.Parser):\n")
    out.write("    def __init__(self, goal{}, builder=None):\n".format(default_goal))
    out.write("        if builder is None:\n")
    out.write("            builder = DefaultBuilder()\n")
    out.write("        super().__init__(actions, ctns, reductions, special_cases, error_codes,\n")
    out.write("                         goal_nt_to_init_state[goal], builder)\n")
    out.write("\n")
