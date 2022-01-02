import unittest

import jsparagus.gen
from jsparagus import parse_pgen, parse_pgen_generated


class ParsePgenTestCase(unittest.TestCase):
    def test_self(self):
        import os
        filename = os.path.join(os.path.dirname(parse_pgen.__file__), "..",
                                "pgen.pgen")
        grammar = parse_pgen.load_grammar(filename)
        self.maxDiff = None
        pgen_grammar = parse_pgen.pgen_grammar
        self.assertEqual(pgen_grammar.nonterminals, grammar.nonterminals)
        self.assertEqual(pgen_grammar.variable_terminals,
                         grammar.variable_terminals)
        self.assertEqual(pgen_grammar.goals(), grammar.goals())

        with open(parse_pgen_generated.__file__) as f:
            pre_generated = f.read()

        import io
        out = io.StringIO()
        jsparagus.gen.generate_parser(out, grammar)
        generated_from_file = out.getvalue()

        self.maxDiff = None
        self.assertEqual(pre_generated, generated_from_file)


if __name__ == '__main__':
    unittest.main()
