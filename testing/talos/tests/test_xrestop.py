#!/usr/bin/env python

"""
Tests for talos.xrestop
"""

import os
import subprocess
import sys
import unittest
from talos.cmanager_linux import xrestop

here = os.path.dirname(os.path.abspath(__file__))
xrestop_output = os.path.join(here, 'xrestop_output.txt')

class TestXrestop(unittest.TestCase):

    def test_parsing(self):
        """test parsing xrestop output from xrestop_output.txt"""

        class MockPopen(object):
            """
            stub class for subprocess.Popen
            We mock this to return a local static copy of xrestop output
            This has the unfortunate nature of depending on implementation
            details.
            """
            def __init__(self, *args, **kwargs):
                self.returncode = 0
            def communicate(self):
                stdout = file(xrestop_output).read()
                return stdout, ''

        # monkey-patch subprocess.Popen
        Popen = subprocess.Popen
        subprocess.Popen = MockPopen

        # get the output
        output = xrestop()

        # ensure that the parsed output is equal to what is in
        # xrestop_output.txt
        self.assertEqual(len(output), 7) # seven windows with PIDs

        # the first window is Thunderbird
        pid = 2035 # thundrbird's pid
        self.assertTrue(pid in output)
        thunderbird = output[pid]
        self.assertEqual(thunderbird['index'], 0)
        self.assertEqual(thunderbird['total bytes'], '~4728761')

        # PID=1668 is a Terminal
        pid = 1668
        self.assertTrue(pid in output)
        terminal = output[pid]
        self.assertEqual(terminal['pixmap bytes'], '1943716')

        # cleanup: set subprocess.Popen back
        subprocess.Popen = Popen

if __name__ == '__main__':
    unittest.main()
