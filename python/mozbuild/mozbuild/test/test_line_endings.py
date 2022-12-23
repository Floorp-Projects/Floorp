import unittest

import mozunit
from mozfile import NamedTemporaryFile
from six import StringIO

from mozbuild.preprocessor import Preprocessor


class TestLineEndings(unittest.TestCase):
    """
    Unit tests for the Context class
    """

    def setUp(self):
        self.pp = Preprocessor()
        self.pp.out = StringIO()
        self.f = NamedTemporaryFile(mode="wb")

    def tearDown(self):
        self.f.close()

    def createFile(self, lineendings):
        for line, ending in zip([b"a", b"#literal b", b"c"], lineendings):
            self.f.write(line + ending)
        self.f.flush()

    def testMac(self):
        self.createFile([b"\x0D"] * 3)
        self.pp.do_include(self.f.name)
        self.assertEqual(self.pp.out.getvalue(), "a\nb\nc\n")

    def testUnix(self):
        self.createFile([b"\x0A"] * 3)
        self.pp.do_include(self.f.name)
        self.assertEqual(self.pp.out.getvalue(), "a\nb\nc\n")

    def testWindows(self):
        self.createFile([b"\x0D\x0A"] * 3)
        self.pp.do_include(self.f.name)
        self.assertEqual(self.pp.out.getvalue(), "a\nb\nc\n")


if __name__ == "__main__":
    mozunit.main()
