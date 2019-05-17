# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

import os, sys, os.path, time
from tempfile import mkdtemp
from shutil import rmtree
import mozunit

from mozbuild.action.buildlist import addEntriesToListFile


class TestBuildList(unittest.TestCase):
  """
  Unit tests for buildlist.py
  """
  def setUp(self):
    self.tmpdir = mkdtemp()

  def tearDown(self):
    rmtree(self.tmpdir)

  # utility methods for tests
  def touch(self, file, dir=None):
    if dir is None:
      dir = self.tmpdir
    f = os.path.join(dir, file)
    open(f, 'w').close()
    return f

  def assertFileContains(self, filename, l):
    """Assert that the lines in the file |filename| are equal
    to the contents of the list |l|, in order."""
    l = l[:]
    f = open(filename, 'r')
    lines = [line.rstrip() for line in f.readlines()]
    f.close()
    for line in lines:
      self.assert_(len(l) > 0,
                   "ran out of expected lines! (expected '{0}', got '{1}')"
                   .format(l, lines))
      self.assertEqual(line, l.pop(0))
    self.assert_(len(l) == 0, 
                 "not enough lines in file! (expected '{0}',"
                 " got '{1}'".format(l, lines))

  def test_basic(self):
    "Test that addEntriesToListFile works when file doesn't exist."
    testfile = os.path.join(self.tmpdir, "test.list")
    l = ["a", "b", "c"]
    addEntriesToListFile(testfile, l)
    self.assertFileContains(testfile, l)
    # ensure that attempting to add the same entries again doesn't change it
    addEntriesToListFile(testfile, l)
    self.assertFileContains(testfile, l)

  def test_append(self):
    "Test adding new entries."
    testfile = os.path.join(self.tmpdir, "test.list")
    l = ["a", "b", "c"]
    addEntriesToListFile(testfile, l)
    self.assertFileContains(testfile, l)
    l2 = ["x","y","z"]
    addEntriesToListFile(testfile, l2)
    l.extend(l2)
    self.assertFileContains(testfile, l)

  def test_append_some(self):
    "Test adding new entries mixed with existing entries."
    testfile = os.path.join(self.tmpdir, "test.list")
    l = ["a", "b", "c"]
    addEntriesToListFile(testfile, l)
    self.assertFileContains(testfile, l)
    addEntriesToListFile(testfile, ["a", "x", "c", "z"])
    self.assertFileContains(testfile, ["a", "b", "c", "x", "z"])

  def test_add_multiple(self):
    """Test that attempting to add the same entry multiple times results in
    only one entry being added."""
    testfile = os.path.join(self.tmpdir, "test.list")
    addEntriesToListFile(testfile, ["a","b","a","a","b"])
    self.assertFileContains(testfile, ["a","b"])
    addEntriesToListFile(testfile, ["c","a","c","b","c"])
    self.assertFileContains(testfile, ["a","b","c"])

if __name__ == '__main__':
  mozunit.main()
