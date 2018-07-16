# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pyparsing import ParseException
import unittest

import cmakeparser as cp

class TestCMakeParser(unittest.TestCase):
    def test_arguments(self):
        self.assertEqual(cp.arguments.parseString('1').asList(), ['1'])
        self.assertEqual(cp.arguments.parseString('(1 2)').asList(),
                         ['(', '1', '2', ')'])

    def test_command(self):
        self.assertEqual(cp.command.parseString('blah()').asList(),
                         [['blah', '(', ')']])
        self.assertEqual(cp.command.parseString('blah(1)').asList(),
                         [['blah', '(', '1', ')']])
        self.assertEqual(cp.command.parseString('blah(1 (2 3))').asList(),
                         [['blah', '(', '1', '(', '2', '3', ')', ')']])

    def test_evaluate_boolean(self):
        self.assertTrue(cp.evaluate_boolean({}, ['TRUE']))
        self.assertFalse(cp.evaluate_boolean({}, ['NOT', 'TRUE']))
        self.assertFalse(cp.evaluate_boolean({}, ['TRUE', 'AND', 'FALSE']))
        self.assertTrue(cp.evaluate_boolean({}, ['ABC', 'MATCHES', '^AB']))
        self.assertTrue(cp.evaluate_boolean({}, ['TRUE', 'OR', 'FALSE']))
        self.assertTrue(cp.evaluate_boolean({}, ['ABC', 'STREQUAL', 'ABC']))
        self.assertTrue(cp.evaluate_boolean({'ABC': '1'}, ['ABC']))
        self.assertFalse(cp.evaluate_boolean({'ABC': '0'}, ['${ABC}']))
        self.assertFalse(cp.evaluate_boolean({}, ['ABC']))
        self.assertFalse(cp.evaluate_boolean({}, ['${ABC}']))
        self.assertTrue(cp.evaluate_boolean({'YES_ABC': 1, 'VAL': 'ABC'},
                                            ['YES_${VAL}']))
        self.assertTrue(cp.evaluate_boolean({'ABC': 'DEF', 'DEF': 1},
                                            ['${ABC}']))
        self.assertTrue(cp.evaluate_boolean({}, ['FALSE', 'OR', '(', 'FALSE', 'OR', 'TRUE', ')']))
        self.assertFalse(cp.evaluate_boolean({}, ['FALSE', 'OR', '(', 'FALSE', 'AND', 'TRUE', ')']))

    def test_foreach(self):
        s = """
            set(STUFF A B C D E F)
            foreach(item ${STUFF})
                set(YES_${item} 1)
            endforeach ()
            """
        parsed = cp.cmake.parseString(s)
        variables = {}
        cp.evaluate(variables, [], parsed)
        for k in ['A', 'B', 'C', 'D', 'E', 'F']:
            self.assertEqual(variables['YES_%s' % k], '1')

        s = """
            set(STUFF "A;B;C;D;E;F")
            foreach(item ${STUFF})
                set(${item} 1)
            endforeach ()
            """
        parsed = cp.cmake.parseString(s)
        variables = {}
        cp.evaluate(variables, [], parsed)
        for k in ['A', 'B', 'C', 'D', 'E', 'F']:
            self.assertEqual(variables[k], '1')

        s = """
            set(STUFF D E F)
            foreach(item A B C ${STUFF})
                set(${item} 1)
            endforeach ()
            """
        parsed = cp.cmake.parseString(s)
        variables = {}
        cp.evaluate(variables, [], parsed)
        for k in ['A', 'B', 'C', 'D', 'E', 'F']:
            self.assertEqual(variables[k], '1')

    def test_list(self):
        s = 'list(APPEND TEST 1 1 2 3 5 8 13)'
        parsed = cp.cmake.parseString(s)
        variables = {}
        cache_variables = []
        cp.evaluate(variables, cache_variables, parsed)
        self.assertEqual(variables['TEST'], '1 1 2 3 5 8 13')
        self.assertEqual(len(cache_variables), 0)

        s = """
            set(TEST 1)
            list(APPEND TEST 1 2 3 5 8 13)
            """
        parsed = cp.cmake.parseString(s)
        variables = {}
        cache_variables = []
        cp.evaluate(variables, cache_variables, parsed)
        self.assertEqual(variables['TEST'], '1 1 2 3 5 8 13')
        self.assertEqual(len(cache_variables), 0)

    def test_malformed_input(self):
        self.assertEqual(len(cp.cmake.parseString('func ((A)')), 0)
        self.assertEqual(len(cp.cmake.parseString('cmd"arg"(arg2)')), 0)
        self.assertRaises(ParseException, cp.command.parseString, 'func ((A)')
        self.assertRaises(ParseException, cp.identifier.parseString,
                          '-asdlf')
        self.assertEqual(cp.identifier.parseString('asd-lf')[0], 'asd')
        self.assertRaises(ParseException, cp.quoted_argument.parseString,
                          'blah"')
        s = """
            " blah blah
            blah
            """
        self.assertRaises(ParseException, cp.quoted_argument.parseString,
                          s)
        self.assertRaises(ParseException, cp.quoted_argument.parseString,
                          'asdlf')
        self.assertRaises(ParseException, cp.unquoted_argument.parseString,
                          '#asdflkj')

    def test_parse_if(self):
        s = """
            if (A)
                B()
            elseif (C)
                D()
            elseif (E)
                F()
            else ()
                H()
            endif ()
            I()
            """
        parsed = cp.cmake.parseString(s)
        self.assertEqual(len(parsed), 10)
        end, conditions = cp.parse_if(parsed, 0)
        self.assertEqual(parsed[end][0], 'endif')
        self.assertEqual(len(conditions), 4)
        self.assertEqual(conditions[0][0], ['A'])
        self.assertEqual(conditions[0][1][0], parsed[1])
        self.assertEqual(conditions[1][0], ['C'])
        self.assertEqual(conditions[1][1][0], parsed[3])
        self.assertEqual(conditions[2][0], ['E'])
        self.assertEqual(conditions[2][1][0], parsed[5])
        self.assertEqual(conditions[3][0], ['TRUE'])
        self.assertEqual(conditions[3][1][0], parsed[7])

    def test_return(self):
        s = """
            set(TEST 2)
            if (true)
                return()
            endif ()
            set(TEST 3)
            """
        parsed = cp.cmake.parseString(s)
        variables = {}
        cache_variables = []
        cp.evaluate(variables, cache_variables, parsed)
        self.assertEqual(variables['TEST'], '2')
        self.assertEqual(len(cache_variables), 0)

    def test_set(self):
        s = """set(TEST 2)"""
        parsed = cp.cmake.parseString(s)
        variables = {}
        cache_variables = []
        cp.evaluate(variables, cache_variables, parsed)
        self.assertEqual(variables['TEST'], '2')
        self.assertEqual(len(cache_variables), 0)

        s = """set(TEST 3 CACHE "documentation")"""
        parsed = cp.cmake.parseString(s)
        variables = {}
        cache_variables = []
        cp.evaluate(variables, cache_variables, parsed)
        self.assertEqual(variables['TEST'], '3')
        self.assertTrue('TEST' in cache_variables)

        s = """set(TEST A B C D)"""
        parsed = cp.cmake.parseString(s)
        variables = {}
        cp.evaluate(variables, [], parsed)
        self.assertEqual(variables['TEST'], 'A B C D')

        s = """set(TEST ${TEST} E F G H)"""
        parsed = cp.cmake.parseString(s)
        cp.evaluate(variables, [], parsed)
        self.assertEqual(variables['TEST'], 'A B C D E F G H')


if __name__ == '__main__':
    unittest.main()
