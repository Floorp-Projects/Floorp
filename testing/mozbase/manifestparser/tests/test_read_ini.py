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

    def parse_manifest(self, string):
        buf = StringIO()
        buf.write(string)
        buf.seek(0)
        return read_ini(buf)

    def test_inline_comments(self):
        result = self.parse_manifest("""
[test_felinicity.py]
kittens = true # This test requires kittens
cats = false#but not cats
""")[0][1]

        # make sure inline comments get stripped out, but comments without a space in front don't
        self.assertEqual(result['kittens'], 'true')
        self.assertEqual(result['cats'], "false#but not cats")

    def test_line_continuation(self):
        result = self.parse_manifest("""
[test_caninicity.py]
breeds =
  sheppard
  retriever
  terrier

[test_cats_and_dogs.py]
  cats=yep
  dogs=
    yep
      yep
birds=nope
  fish=nope
""")
        self.assertEqual(result[0][1]['breeds'].split(), ['sheppard', 'retriever', 'terrier'])
        self.assertEqual(result[1][1]['cats'], 'yep')
        self.assertEqual(result[1][1]['dogs'].split(), ['yep', 'yep'])
        self.assertEqual(result[1][1]['birds'].split(), ['nope', 'fish=nope'])


if __name__ == '__main__':
    mozunit.main()
