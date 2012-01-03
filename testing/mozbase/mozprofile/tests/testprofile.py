#!/usr/bin/env python

import os
import shutil
import subprocess
import tempfile
import unittest
from mozprofile.prefs import Preferences
from mozprofile.profile import Profile

class ProfileTest(unittest.TestCase):
    """test mozprofile"""

    def run_command(self, *args):
        """
        runs mozprofile;
        returns (stdout, stderr, code)
        """
        process = subprocess.Popen(args,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
        stdout = stdout.strip()
        stderr = stderr.strip()
        return stdout, stderr, process.returncode

    def compare_generated(self, _prefs, commandline):
        """
        writes out to a new profile with mozprofile command line
        reads the generated preferences with prefs.py
        compares the results
        cleans up
        """
        profile, stderr, code = self.run_command(*commandline)
        prefs_file = os.path.join(profile, 'user.js')
        self.assertTrue(os.path.exists(prefs_file))
        read = Preferences.read_prefs(prefs_file)
        if isinstance(_prefs, dict):
            read = dict(read)
        self.assertEqual(_prefs, read)
        shutil.rmtree(profile)

    def test_basic_prefs(self):
        _prefs = {"browser.startup.homepage": "http://planet.mozilla.org/"}
        commandline = ["mozprofile"]
        _prefs = _prefs.items()
        for pref, value in _prefs:
            commandline += ["--pref", "%s:%s" % (pref, value)]
        self.compare_generated(_prefs, commandline)

    def test_ordered_prefs(self):
        """ensure the prefs stay in the right order"""
        _prefs = [("browser.startup.homepage", "http://planet.mozilla.org/"),
                  ("zoom.minPercent", 30),
                  ("zoom.maxPercent", 300),
                  ("webgl.verbose", 'false')]
        commandline = ["mozprofile"]
        for pref, value in _prefs:
            commandline += ["--pref", "%s:%s" % (pref, value)]
        _prefs = [(i, Preferences.cast(j)) for i, j in _prefs]
        self.compare_generated(_prefs, commandline)

    def test_ini(self):

        # write the .ini file
        _ini = """[DEFAULT]
browser.startup.homepage = http://planet.mozilla.org/

[foo]
browser.startup.homepage = http://github.com/
"""
        fd, name = tempfile.mkstemp(suffix='.ini')
        os.write(fd, _ini)
        os.close(fd)
        commandline = ["mozprofile", "--preferences", name]

        # test the [DEFAULT] section
        _prefs = {'browser.startup.homepage': 'http://planet.mozilla.org/'}
        self.compare_generated(_prefs, commandline)

        # test a specific section
        _prefs = {'browser.startup.homepage': 'http://github.com/'}
        commandline[-1] = commandline[-1] + ':foo'
        self.compare_generated(_prefs, commandline)

        # cleanup
        os.remove(name)

    def test_magic_markers(self):
        """ensure our magic markers are working"""

        profile = Profile()
        prefs_file = os.path.join(profile.profile, 'user.js')

        # we shouldn't have any initial preferences
        initial_prefs = Preferences.read_prefs(prefs_file)
        self.assertFalse(initial_prefs)
        initial_prefs = file(prefs_file).read().strip()
        self.assertFalse(initial_prefs)

        # add some preferences
        prefs1 = [("browser.startup.homepage", "http://planet.mozilla.org/"),
                   ("zoom.minPercent", 30)]
        profile.set_preferences(prefs1)
        self.assertEqual(prefs1, Preferences.read_prefs(prefs_file))
        lines = file(prefs_file).read().strip().splitlines()
        self.assertTrue('#MozRunner Prefs Start' in lines)
        self.assertTrue('#MozRunner Prefs End' in lines)

        # add some more preferences
        prefs2 = [("zoom.maxPercent", 300),
                   ("webgl.verbose", 'false')]
        profile.set_preferences(prefs2)
        self.assertEqual(prefs1 + prefs2, Preferences.read_prefs(prefs_file))
        lines = file(prefs_file).read().strip().splitlines()
        self.assertTrue(lines.count('#MozRunner Prefs Start') == 2)
        self.assertTrue(lines.count('#MozRunner Prefs End') == 2)

        # now clean it up
        profile.clean_preferences()
        final_prefs = Preferences.read_prefs(prefs_file)
        self.assertFalse(final_prefs)
        lines = file(prefs_file).read().strip().splitlines()
        self.assertTrue('#MozRunner Prefs Start' not in lines)
        self.assertTrue('#MozRunner Prefs End' not in lines)        

    def test_json(self):
        _prefs = {"browser.startup.homepage": "http://planet.mozilla.org/"}
        json = '{"browser.startup.homepage": "http://planet.mozilla.org/"}'

        # just repr it...could use the json module but we don't need it here
        fd, name = tempfile.mkstemp(suffix='.json')
        os.write(fd, json)
        os.close(fd)

        commandline = ["mozprofile", "--preferences", name]
        self.compare_generated(_prefs, commandline)


if __name__ == '__main__':
    unittest.main()
