"""generate_js_parser_tables.py - Generate tables from the ES grammar."""

import argparse
import os
import jsparagus.gen
import jsparagus.grammar
from . import load_es_grammar


def hack_grammar(g):
    # We throw away most of the boolean parameters in the grammar, as the
    # current parser generator's approach of fully expanding them is a huge
    # pain.

    PARAM_WHITELIST = ['In', 'Default']

    def filter_params(params):
        return tuple(p for p in params if p in PARAM_WHITELIST)

    def filter_args(args):
        return tuple(pair for pair in args if pair[0] in PARAM_WHITELIST)

    def filter_element(e):
        """ Strip nt arguments. """
        if isinstance(e, jsparagus.grammar.Nt):
            return jsparagus.grammar.Nt(e.name, filter_args(e.args))
        elif isinstance(e, jsparagus.grammar.Optional):
            return jsparagus.grammar.Optional(filter_element(e.inner))
        else:
            return e

    def filter_condition(c):
        if c is None or c[0] not in PARAM_WHITELIST:
            return None
        return c

    def filter_production(p):
        """ Discard production conditions and nt arguments. """
        body = [filter_element(e) for e in p.body]
        return jsparagus.grammar.Production(body, p.reducer,
                                            condition=filter_condition(p.condition))

    nonterminals = {}
    for nt, nt_def in g.nonterminals.items():
        params = tuple(filter_params(nt_def.params))
        rhs_list = [filter_production(p) for p in nt_def.rhs_list]
        nonterminals[nt] = jsparagus.grammar.NtDef(params, rhs_list, nt_def.type)
    return g.with_nonterminals(nonterminals)


def main():
    # Read command-line options.
    parser = argparse.ArgumentParser(
        description='Ponder the ECMAScript grammar.',
        allow_abbrev=False)
    default_filename = os.path.join(os.path.dirname(__file__),
                                    "es-simplified.esgrammar")
    parser.add_argument(
        'filename', metavar='FILE', nargs='?', default=default_filename,
        help=".esgrammar (or .jsparagus_dump) input file")
    parser.add_argument(
        'handler_info', metavar='HANDLER_INFO', nargs='?',
        help="JSON file that contains information about handler")
    parser.add_argument(
        '-e', '--extend', action='append', default=[],
        help="name of a files which contains a grammar_extension Rust macro.")
    parser.add_argument(
        '-o', '--output', metavar='FILE', default='/dev/stdout',
        help="output filename for parser tables")
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help="print some debug output")
    parser.add_argument(
        '--progress', action='store_true',
        help="print a dot each time a state is analyzed (thousands of them)")
    parser.add_argument(
        '--debug', action='store_true',
        help="annotate the generated code with grammar productions")
    args = parser.parse_args()

    # Check filenames.
    in_filename = args.filename
    if in_filename.endswith('.esgrammar'):
        from_source = True
    elif in_filename.endswith('.jsparagus_dump'):
        from_source = False
    else:
        raise ValueError("input file extension should be .esgrammar or .jsparagus_dump")

    out_filename = args.output
    if out_filename.endswith('.py'):
        target = 'python'
    elif out_filename.endswith('.rs'):
        target = 'rust'
    elif out_filename.endswith('.jsparagus_dump'):
        target = 'dump'
    else:
        raise ValueError("-o file extension should be .py, .rs, or .jsparagus_dump")

    in_extend = args.extend
    if from_source:
        assert all(f.endswith('.rs') for f in in_extend), "Extension are only supposed to be Rust files."
    else:
        assert in_extend == [], "Cannot add extensions to the generated parse table."

    # Load input and analyze it.
    if from_source:
        grammar = load_es_grammar.load_syntactic_grammar(in_filename, in_extend)
        grammar = hack_grammar(grammar)
        if args.verbose:
            grammar.dump()

        states = jsparagus.gen.generate_parser_states(
            grammar, verbose=args.verbose, progress=args.progress)
    else:
        states = jsparagus.gen.ParseTable.load(in_filename)

    # Generate output.
    try:
        if target in ('python', 'rust'):
            with open(out_filename, 'w') as f:
                jsparagus.gen.generate_parser(f, states,
                                              target=target,
                                              verbose=args.verbose,
                                              debug=args.debug,
                                              handler_info=args.handler_info)
        else:
            assert target == 'dump'
            states.save(out_filename)
    except Exception:
        # On failure, don't leave a partial output file lying around.
        try:
            os.remove(out_filename)
        except Exception:
            pass
        raise


if __name__ == '__main__':
    main()
