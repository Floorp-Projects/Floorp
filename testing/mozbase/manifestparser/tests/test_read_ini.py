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
from ConfigParser import ConfigParser
from StringIO import StringIO

import mozunit


class IniParserTest(unittest.TestCase):

    def test_inline_comments(self):
        """
        We have no inline comments; so we're testing to ensure we don't:
        https://bugzilla.mozilla.org/show_bug.cgi?id=855288
        """

        # test '#' inline comments (really, the lack thereof)
        string = """[test_felinicity.py]
kittens = true # This test requires kittens
"""
        buffer = StringIO()
        buffer.write(string)
        buffer.seek(0)
        result = read_ini(buffer)[0][1]['kittens']
        self.assertEqual(result, "true # This test requires kittens")

        # compare this to ConfigParser
        # python 2.7 ConfigParser does not support '#' as an
        # inline comment delimeter (for "backwards compatability"):
        # http://docs.python.org/2/library/configparser.html
        buffer.seek(0)
        parser = ConfigParser()
        parser.readfp(buffer)
        control = parser.get('test_felinicity.py', 'kittens')
        self.assertEqual(result, control)

        # test ';' inline comments (really, the lack thereof)
        string = string.replace('#', ';')
        buffer = StringIO()
        buffer.write(string)
        buffer.seek(0)
        result = read_ini(buffer)[0][1]['kittens']
        self.assertEqual(result, "true ; This test requires kittens")

        # compare this to ConfigParser
        # python 2.7 ConfigParser *does* support ';' as an
        # inline comment delimeter (ibid).
        # Python 3.x configparser, OTOH, does not support
        # inline-comments by default.  It does support their specification,
        # though they are weakly discouraged:
        # http://docs.python.org/dev/library/configparser.html
        buffer.seek(0)
        parser = ConfigParser()
        parser.readfp(buffer)
        control = parser.get('test_felinicity.py', 'kittens')
        self.assertNotEqual(result, control)


if __name__ == '__main__':
    mozunit.main()
