#!/usr/bin/env python

"""
test .ini parsing

ensure our .ini parser is doing what we want; to be deprecated for
python's standard ConfigParser when 2.7 is reality so OrderedDict
is the default:

http://docs.python.org/2/library/configparser.html
"""

import unittest
from manifestparser import read_ini
from StringIO import StringIO

import mozunit


class IniParserTest(unittest.TestCase):

    def test_inline_comments(self):
        manifest = """
[test_felinicity.py]
kittens = true # This test requires kittens
cats = false#but not cats
"""
        # make sure inline comments get stripped out, but comments without a space in front don't
        buf = StringIO()
        buf.write(manifest)
        buf.seek(0)
        result = read_ini(buf)[0][1]
        self.assertEqual(result['kittens'], 'true')
        self.assertEqual(result['cats'], "false#but not cats")


if __name__ == '__main__':
    mozunit.main()
