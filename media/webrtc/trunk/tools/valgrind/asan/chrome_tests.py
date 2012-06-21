#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Runs various chrome tests through asan_test.py.

Most of this code is copied from ../valgrind/chrome_tests.py.
TODO(glider): put common functions to a standalone module.
'''

import glob
import logging
import optparse
import os
import stat
import sys

import logging_utils
import path_utils

import common
import asan_test

class TestNotFound(Exception): pass

def Dir2IsNewer(dir1, dir2):
  if dir2 is None or not os.path.isdir(dir2):
    return False
  if dir1 is None or not os.path.isdir(dir1):
    return True
  return os.stat(dir2)[stat.ST_MTIME] > os.stat(dir1)[stat.ST_MTIME]

def FindNewestDir(dirs):
  newest_dir = None
  for dir in dirs:
    if Dir2IsNewer(newest_dir, dir):
      newest_dir = dir
  return newest_dir

def File2IsNewer(file1, file2):
  if file2 is None or not os.path.isfile(file2):
    return False
  if file1 is None or not os.path.isfile(file1):
    return True
  return os.stat(file2)[stat.ST_MTIME] > os.stat(file1)[stat.ST_MTIME]

def FindDirContainingNewestFile(dirs, file):
  """Searches for the directory containing the newest copy of |file|.

  Args:
    dirs: A list of paths to the directories to search among.
    file: A string containing the file name to search.

  Returns:
    The string representing the the directory containing the newest copy of
    |file|.

  Raises:
    IOError: |file| was not found.
  """
  newest_dir = None
  newest_file = None
  for dir in dirs:
    the_file = os.path.join(dir, file)
    if File2IsNewer(newest_file, the_file):
      newest_dir = dir
      newest_file = the_file
  if newest_dir is None:
    raise IOError("cannot find file %s anywhere, have you built it?" % file)
  return newest_dir

class ChromeTests(object):
  '''This class is derived from the chrome_tests.py file in ../purify/.
  '''

  def __init__(self, options, args, test):
    # The known list of tests.
    # Recognise the original abbreviations as well as full executable names.
    self._test_list = {
      "base": self.TestBase,            "base_unittests": self.TestBase,
      "browser": self.TestBrowser,      "browser_tests": self.TestBrowser,
      "crypto": self.TestCrypto,        "crypto_unittests": self.TestCrypto,
      "googleurl": self.TestGURL,       "googleurl_unittests": self.TestGURL,
      "content": self.TestContent,      "content_unittests": self.TestContent,
      "courgette": self.TestCourgette,
      "courgette_unittests": self.TestCourgette,
      "ipc": self.TestIpc,              "ipc_tests": self.TestIpc,
      "layout": self.TestLayout,        "layout_tests": self.TestLayout,
      "media": self.TestMedia,          "media_unittests": self.TestMedia,
      "net": self.TestNet,              "net_unittests": self.TestNet,
      "printing": self.TestPrinting,    "printing_unittests": self.TestPrinting,
      "remoting": self.TestRemoting,    "remoting_unittests": self.TestRemoting,
      "startup": self.TestStartup,      "startup_tests": self.TestStartup,
      "sync": self.TestSync,            "sync_unit_tests": self.TestSync,
      "test_shell": self.TestTestShell, "test_shell_tests": self.TestTestShell,
      "ui": self.TestUI,                "ui_tests": self.TestUI,
      "unit": self.TestUnit,            "unit_tests": self.TestUnit,
      "views": self.TestViews,          "views_unittests": self.TestViews,
      "sql": self.TestSql,              "sql_unittests": self.TestSql,
      "ui_unit": self.TestUIUnit,       "ui_unittests": self.TestUIUnit,
      "gfx": self.TestGfx,              "gfx_unittests": self.TestGfx,
    }

    if test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    self._options = options
    self._args = args
    self._test = test

    script_dir = path_utils.ScriptDir()

    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/asan/
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))

    # Since this path is used for string matching, make sure it's always
    # an absolute Unix-style path.
    self._source_dir = os.path.abspath(self._source_dir).replace('\\', '/')

    asan_test_script = os.path.join(script_dir, "asan_test.py")
    self._command_preamble = [asan_test_script]

  def _DefaultCommand(self, module, exe=None, asan_test_args=None):
    '''Generates the default command array that most tests will use.

    Args:
      module: The module name (corresponds to the dir in src/ where the test
              data resides).
      exe: The executable name.
      asan_test_args: additional arguments to append to the command line.
    Returns:
      A string with the command to run the test.
    '''
    if not self._options.build_dir:
      dirs = [
        os.path.join(self._source_dir, "xcodebuild", "Debug"),
        os.path.join(self._source_dir, "out", "Debug"),
      ]
      if exe:
        self._options.build_dir = FindDirContainingNewestFile(dirs, exe)
      else:
        self._options.build_dir = FindNewestDir(dirs)

    cmd = list(self._command_preamble)

    if asan_test_args != None:
      for arg in asan_test_args:
        cmd.append(arg)
    if exe:
      cmd.append(os.path.join(self._options.build_dir, exe))
      # Show elapased time so we can find the slowpokes.
      cmd.append("--gtest_print_time")
    if self._options.gtest_repeat:
      cmd.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    return cmd

  def Suppressions(self):
    '''Builds the list of available suppressions files.'''
    ret = []
    directory = path_utils.ScriptDir()
    suppression_file = os.path.join(directory, "suppressions.txt")
    if os.path.exists(suppression_file):
      ret.append(suppression_file)
    suppression_file = os.path.join(directory, "suppressions_linux.txt")
    if os.path.exists(suppression_file):
      ret.append(suppression_file)
    return ret

  def Run(self):
    '''Runs the test specified by command-line argument --test.'''
    logging.info("running test %s" % (self._test))
    return self._test_list[self._test]()

  def _ReadGtestFilterFile(self, name, cmd):
    '''Reads files which contain lists of tests to filter out with
    --gtest_filter and appends the command-line option to |cmd|.

    Args:
      name: the test executable name.
      cmd: the test running command line to be modified.
    '''
    filters = []
    directory = path_utils.ScriptDir()
    gtest_filter_files = [
        os.path.join(directory, name + ".gtest-asan.txt"),
        # TODO(glider): Linux vs. CrOS?
    ]
    logging.info("Reading gtest exclude filter files:")
    for filename in gtest_filter_files:
      # strip the leading absolute path (may be very long on the bot)
      # and the following / or \.
      readable_filename = filename.replace(self._source_dir, "")[1:]
      if not os.path.exists(filename):
        logging.info("  \"%s\" - not found" % readable_filename)
        continue
      logging.info("  \"%s\" - OK" % readable_filename)
      f = open(filename, 'r')
      for line in f.readlines():
        if line.startswith("#") or line.startswith("//") or line.isspace():
          continue
        line = line.rstrip()
        filters.append(line)
    gtest_filter = self._options.gtest_filter
    if len(filters):
      if gtest_filter:
        gtest_filter += ":"
        if gtest_filter.find("-") < 0:
          gtest_filter += "-"
      else:
        gtest_filter = "-"
      gtest_filter += ":".join(filters)
    if gtest_filter:
      cmd.append("--gtest_filter=%s" % gtest_filter)

  def SimpleTest(self, module, name, asan_test_args=None, cmd_args=None):
    '''Builds the command line and runs the specified test.

    Args:
      module: The module name (corresponds to the dir in src/ where the test
              data resides).
      name: The executable name.
      asan_test_args: Additional command line args for asan.
      cmd_args: Additional command line args for the test.
    '''
    cmd = self._DefaultCommand(module, name, asan_test_args)
    supp = self.Suppressions()
    self._ReadGtestFilterFile(name, cmd)
    if cmd_args:
      cmd.extend(["--"])
      cmd.extend(cmd_args)

    # Sets LD_LIBRARY_PATH to the build folder so external libraries can be
    # loaded.
    if (os.getenv("LD_LIBRARY_PATH")):
      os.putenv("LD_LIBRARY_PATH", "%s:%s" % (os.getenv("LD_LIBRARY_PATH"),
                                              self._options.build_dir))
    else:
      os.putenv("LD_LIBRARY_PATH", self._options.build_dir)
    return asan_test.RunTool(cmd, supp, module)

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests")

  def TestBrowser(self):
    return self.SimpleTest("chrome", "browser_tests")

  def TestCrypto(self):
    return self.SimpleTest("crypto", "crypto_unittests")

  def TestGURL(self):
    return self.SimpleTest("chrome", "googleurl_unittests")

  def TestContent(self):
    return self.SimpleTest("content", "content_unittests")

  def TestCourgette(self):
    return self.SimpleTest("courgette", "courgette_unittests")

  def TestMedia(self):
    return self.SimpleTest("chrome", "media_unittests")

  def TestPrinting(self):
    return self.SimpleTest("chrome", "printing_unittests")

  def TestRemoting(self):
    return self.SimpleTest("chrome", "remoting_unittests")

  def TestSync(self):
    return self.SimpleTest("chrome", "sync_unit_tests")

  def TestIpc(self):
    return self.SimpleTest("ipc", "ipc_tests")

  def TestNet(self):
    return self.SimpleTest("net", "net_unittests")

  def TestStartup(self):
    # We don't need the performance results, we're just looking for pointer
    # errors, so set number of iterations down to the minimum.
    os.putenv("STARTUP_TESTS_NUMCYCLES", "1")
    logging.info("export STARTUP_TESTS_NUMCYCLES=1");
    return self.SimpleTest("chrome", "startup_tests")

  def TestTestShell(self):
    return self.SimpleTest("webkit", "test_shell_tests")

  def TestUnit(self):
    return self.SimpleTest("chrome", "unit_tests")

  def TestViews(self):
    return self.SimpleTest("views", "views_unittests")

  def TestSql(self):
    return self.SimpleTest("chrome", "sql_unittests")

  def TestUIUnit(self):
    return self.SimpleTest("chrome", "ui_unittests")

  def TestGfx(self):
    return self.SimpleTest("chrome", "gfx_unittests")

  def TestUI(self):
    return self.SimpleTest("chrome", "ui_tests",
                           cmd_args=[
                            "--ui-test-action-timeout=80000",
                            "--ui-test-action-max-timeout=180000"])

  def TestLayoutChunk(self, chunk_num, chunk_size):
    '''Runs tests [chunk_num*chunk_size .. (chunk_num+1)*chunk_size).

    Wrap around to beginning of list at end. If chunk_size is zero, run all
    tests in the list once. If a text file is given as argument, it is used as
    the list of tests.
    '''
    # Build the ginormous commandline in 'cmd'.
    # It's going to be roughly
    #  python asan_test.py ... python run_webkit_tests.py ...
    # but we'll use the --indirect flag to asan_test.py
    # to avoid asaning python.
    # Start by building the asan_test.py commandline.
    cmd = self._DefaultCommand("webkit")

    # Now build script_cmd, the run_webkits_tests.py commandline
    # Store each chunk in its own directory so that we can find the data later
    chunk_dir = os.path.join("layout", "chunk_%05d" % chunk_num)
    test_shell = os.path.join(self._options.build_dir, "test_shell")
    out_dir = os.path.join(path_utils.ScriptDir(), "latest")
    out_dir = os.path.join(out_dir, chunk_dir)
    if os.path.exists(out_dir):
      old_files = glob.glob(os.path.join(out_dir, "*.txt"))
      for f in old_files:
        os.remove(f)
    else:
      os.makedirs(out_dir)

    script = os.path.join(self._source_dir, "webkit", "tools", "layout_tests",
                          "run_webkit_tests.py")
    script_cmd = ["python", script, "--run-singly", "-v",
                  "--noshow-results", "--time-out-ms=200000",
                  "--nocheck-sys-deps"]

    # Pass build mode to run_webkit_tests.py.  We aren't passed it directly,
    # so parse it out of build_dir.  run_webkit_tests.py can only handle
    # the two values "Release" and "Debug".
    # TODO(Hercules): unify how all our scripts pass around build mode
    # (--mode / --target / --build_dir / --debug)
    if self._options.build_dir.endswith("Debug"):
      script_cmd.append("--debug");
    if (chunk_size > 0):
      script_cmd.append("--run-chunk=%d:%d" % (chunk_num, chunk_size))
    if len(self._args):
      # if the arg is a txt file, then treat it as a list of tests
      if os.path.isfile(self._args[0]) and self._args[0][-4:] == ".txt":
        script_cmd.append("--test-list=%s" % self._args[0])
      else:
        script_cmd.extend(self._args)
    self._ReadGtestFilterFile("layout", script_cmd)

    # Now run script_cmd with the wrapper in cmd
    cmd.extend(["--"])
    cmd.extend(script_cmd)
    supp = self.Suppressions()
    return asan_test.RunTool(cmd, supp, "layout")

  def TestLayout(self):
    '''Runs the layout tests.'''
    # A "chunk file" is maintained in the local directory so that each test
    # runs a slice of the layout tests of size chunk_size that increments with
    # each run.  Since tests can be added and removed from the layout tests at
    # any time, this is not going to give exact coverage, but it will allow us
    # to continuously run small slices of the layout tests under purify rather
    # than having to run all of them in one shot.
    chunk_size = self._options.num_tests
    if (chunk_size == 0):
      return self.TestLayoutChunk(0, 0)
    chunk_num = 0
    chunk_file = os.path.join("asan_layout_chunk.txt")

    logging.info("Reading state from " + chunk_file)
    try:
      f = open(chunk_file)
      if f:
        str = f.read()
        if len(str):
          chunk_num = int(str)
        # This should be enough so that we have a couple of complete runs
        # of test data stored in the archive (although note that when we loop
        # that we almost guaranteed won't be at the end of the test list)
        if chunk_num > 10000:
          chunk_num = 0
        f.close()
    except IOError, (errno, strerror):
      logging.error("error reading from file %s (%d, %s)" % (chunk_file,
                    errno, strerror))
    ret = self.TestLayoutChunk(chunk_num, chunk_size)

    # Wait until after the test runs to completion to write out the new chunk
    # number.  This way, if the bot is killed, we'll start running again from
    # the current chunk rather than skipping it.
    logging.info("Saving state to " + chunk_file)
    try:
      f = open(chunk_file, "w")
      chunk_num += 1
      f.write("%d" % chunk_num)
      f.close()
    except IOError, (errno, strerror):
      logging.error("error writing to file %s (%d, %s)" % (chunk_file, errno,
                    strerror))

    # Since we're running small chunks of the layout tests, it's important to
    # mark the ones that have errors in them.  These won't be visible in the
    # summary list for long, but will be useful for someone reviewing this bot.
    return ret


def main():
  if not sys.platform.startswith(('linux', 'darwin')):
    logging.error("AddressSanitizer works only on Linux and Mac OS "
                  "at the moment.")
    return 1
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()
  parser.add_option("-b", "--build_dir",
                    help="the location of the output of the compiler output")
  parser.add_option("-t", "--test", action="append",
                    help="which test to run")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  # My machine can do about 120 layout tests/hour in release mode.
  # Let's do 30 minutes worth per run.
  # The CPU is mostly idle, so perhaps we can raise this when
  # we figure out how to run them more efficiently.
  parser.add_option("-n", "--num_tests", default=60, type="int",
                    help="for layout tests: # of subtests per run.  0 for all.")

  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test or not len(options.test):
    parser.error("--test not specified")

  for t in options.test:
    tests = ChromeTests(options, args, t)
    ret = tests.Run()
    if ret:
      return ret
  return 0


if __name__ == "__main__":
  sys.exit(main())
