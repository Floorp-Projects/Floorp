# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.makeutil import (
    Makefile,
    Rule,
)
from mozunit import main
import os
import unittest
from StringIO import StringIO


class TestMakefile(unittest.TestCase):
    def test_rule(self):
        out = StringIO()
        rule = Rule()
        rule.dump(out)
        self.assertEqual(out.getvalue(), '')
        out.truncate(0)

        rule.add_targets(['foo', 'bar'])
        rule.dump(out)
        self.assertEqual(out.getvalue(), 'foo bar:\n')
        out.truncate(0)

        rule.add_targets(['baz'])
        rule.add_dependencies(['qux', 'hoge', 'piyo'])
        rule.dump(out)
        self.assertEqual(out.getvalue(), 'foo bar baz: qux hoge piyo\n')
        out.truncate(0)

        rule = Rule(['foo', 'bar'])
        rule.add_dependencies(['baz'])
        rule.add_commands(['echo $@'])
        rule.add_commands(['$(BAZ) -o $@ $<', '$(TOUCH) $@'])
        rule.dump(out)
        self.assertEqual(out.getvalue(),
            'foo bar: baz\n' +
            '\techo $@\n' +
            '\t$(BAZ) -o $@ $<\n' +
            '\t$(TOUCH) $@\n')

    def test_makefile(self):
        out = StringIO()
        mk = Makefile()
        rule = mk.create_rule(['foo'])
        rule.add_dependencies(['bar', 'baz', 'qux'])
        rule.add_commands(['echo foo'])
        rule = mk.create_rule().add_targets(['bar', 'baz'])
        rule.add_dependencies(['hoge'])
        rule.add_commands(['echo $@'])
        mk.dump(out, removal_guard=False)
        self.assertEqual(out.getvalue(),
            'foo: bar baz qux\n' +
            '\techo foo\n' +
            'bar baz: hoge\n' +
            '\techo $@\n')
        out.truncate(0)

        mk.dump(out)
        self.assertEqual(out.getvalue(),
            'foo: bar baz qux\n' +
            '\techo foo\n' +
            'bar baz: hoge\n' +
            '\techo $@\n' +
            'hoge qux:\n')

    def test_statement(self):
        out = StringIO()
        mk = Makefile()
        mk.create_rule(['foo']).add_dependencies(['bar']) \
                               .add_commands(['echo foo'])
        mk.add_statement('BAR = bar')
        mk.create_rule(['$(BAR)']).add_commands(['echo $@'])
        mk.dump(out, removal_guard=False)
        self.assertEqual(out.getvalue(),
            'foo: bar\n' +
            '\techo foo\n' +
            'BAR = bar\n' +
            '$(BAR):\n' +
            '\techo $@\n')

    @unittest.skipIf(os.name != 'nt', 'Test only applicable on Windows.')
    def test_path_normalization(self):
        out = StringIO()
        mk = Makefile()
        rule = mk.create_rule(['c:\\foo'])
        rule.add_dependencies(['c:\\bar', 'c:\\baz\\qux'])
        rule.add_commands(['echo c:\\foo'])
        mk.dump(out)
        self.assertEqual(out.getvalue(),
            'c:/foo: c:/bar c:/baz/qux\n' +
            '\techo c:\\foo\n' +
            'c:/bar c:/baz/qux:\n')

if __name__ == '__main__':
    main()
