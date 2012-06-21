#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Runs various chrome tests through valgrind_test.py.'''

import glob
import logging
import optparse
import os
import stat
import sys

import logging_utils
import path_utils

import common
import valgrind_test

class TestNotFound(Exception): pass

class MultipleGTestFiltersSpecified(Exception): pass

class BuildDirNotFound(Exception): pass

class BuildDirAmbiguous(Exception): pass

class ChromeTests:
  SLOW_TOOLS = ["memcheck", "tsan", "tsan_rv", "drmemory"]

  def __init__(self, options, args, test):
    if ':' in test:
      (self._test, self._gtest_filter) = test.split(':', 1)
    else:
      self._test = test
      self._gtest_filter = options.gtest_filter

    if self._test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    if options.gtest_filter and options.gtest_filter != self._gtest_filter:
      raise MultipleGTestFiltersSpecified("Can not specify both --gtest_filter "
                                          "and --test %s" % test)

    self._options = options
    self._args = args

    script_dir = path_utils.ScriptDir()
    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/valgrind/
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))
    # since this path is used for string matching, make sure it's always
    # an absolute Unix-style path
    self._source_dir = os.path.abspath(self._source_dir).replace('\\', '/')
    valgrind_test_script = os.path.join(script_dir, "valgrind_test.py")
    self._command_preamble = ["--source_dir=%s" % (self._source_dir)]

    if not self._options.build_dir:
      dirs = [
        os.path.join(self._source_dir, "xcodebuild", "Debug"),
        os.path.join(self._source_dir, "out", "Debug"),
        os.path.join(self._source_dir, "build", "Debug"),
      ]
      build_dir = [d for d in dirs if os.path.isdir(d)]
      if len(build_dir) > 1:
        raise BuildDirAmbiguous("Found more than one suitable build dir:\n"
                                "%s\nPlease specify just one "
                                "using --build_dir" % ", ".join(build_dir))
      elif build_dir:
        self._options.build_dir = build_dir[0]
      else:
        self._options.build_dir = None

    if self._options.build_dir:
      build_dir = os.path.abspath(self._options.build_dir)
      self._command_preamble += ["--build_dir=%s" % (self._options.build_dir)]

  def _EnsureBuildDirFound(self):
    if not self._options.build_dir:
      raise BuildDirNotFound("Oops, couldn't find a build dir, please "
                             "specify it manually using --build_dir")

  def _DefaultCommand(self, tool, exe=None, valgrind_test_args=None):
    '''Generates the default command array that most tests will use.'''
    if exe and common.IsWindows():
      exe += '.exe'

    cmd = list(self._command_preamble)

    # Find all suppressions matching the following pattern:
    # tools/valgrind/TOOL/suppressions[_PLATFORM].txt
    # and list them with --suppressions= prefix.
    script_dir = path_utils.ScriptDir()
    tool_name = tool.ToolName();
    suppression_file = os.path.join(script_dir, tool_name, "suppressions.txt")
    if os.path.exists(suppression_file):
      cmd.append("--suppressions=%s" % suppression_file)
    # Platform-specific suppression
    for platform in common.PlatformNames():
      platform_suppression_file = \
          os.path.join(script_dir, tool_name, 'suppressions_%s.txt' % platform)
      if os.path.exists(platform_suppression_file):
        cmd.append("--suppressions=%s" % platform_suppression_file)

    if self._options.valgrind_tool_flags:
      cmd += self._options.valgrind_tool_flags.split(" ")
    if self._options.keep_logs:
      cmd += ["--keep_logs"]
    if valgrind_test_args != None:
      for arg in valgrind_test_args:
        cmd.append(arg)
    if exe:
      self._EnsureBuildDirFound()
      cmd.append(os.path.join(self._options.build_dir, exe))
      # Valgrind runs tests slowly, so slow tests hurt more; show elapased time
      # so we can find the slowpokes.
      cmd.append("--gtest_print_time")
    if self._options.gtest_repeat:
      cmd.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    return cmd

  def Run(self):
    ''' Runs the test specified by command-line argument --test '''
    logging.info("running test %s" % (self._test))
    return self._test_list[self._test](self)

  def _AppendGtestFilter(self, tool, name, cmd):
    '''Append an appropriate --gtest_filter flag to the googletest binary
       invocation.
       If the user passed his own filter mentioning only one test, just use it.
       Othewise, filter out tests listed in the appropriate gtest_exclude files.
    '''
    if (self._gtest_filter and
        ":" not in self._gtest_filter and
        "?" not in self._gtest_filter and
        "*" not in self._gtest_filter):
      cmd.append("--gtest_filter=%s" % self._gtest_filter)
      return

    filters = []
    gtest_files_dir = os.path.join(path_utils.ScriptDir(), "gtest_exclude")

    gtest_filter_files = [
        os.path.join(gtest_files_dir, name + ".gtest-%s.txt" % tool.ToolName())]
    # Use ".gtest.txt" files only for slow tools, as they now contain
    # Valgrind- and Dr.Memory-specific filters.
    # TODO(glider): rename the files to ".gtest_slow.txt"
    if tool.ToolName() in ChromeTests.SLOW_TOOLS:
      gtest_filter_files += [os.path.join(gtest_files_dir, name + ".gtest.txt")]
    for platform_suffix in common.PlatformNames():
      gtest_filter_files += [
        os.path.join(gtest_files_dir, name + ".gtest_%s.txt" % platform_suffix),
        os.path.join(gtest_files_dir, name + ".gtest-%s_%s.txt" % \
            (tool.ToolName(), platform_suffix))]
    logging.info("Reading gtest exclude filter files:")
    for filename in gtest_filter_files:
      # strip the leading absolute path (may be very long on the bot)
      # and the following / or \.
      readable_filename = filename.replace("\\", "/")  # '\' on Windows
      readable_filename = readable_filename.replace(self._source_dir, "")[1:]
      if not os.path.exists(filename):
        logging.info("  \"%s\" - not found" % readable_filename)
        continue
      logging.info("  \"%s\" - OK" % readable_filename)
      f = open(filename, 'r')
      for line in f.readlines():
        if line.startswith("#") or line.startswith("//") or line.isspace():
          continue
        line = line.rstrip()
        test_prefixes = ["FLAKY", "FAILS"]
        for p in test_prefixes:
          # Strip prefixes from the test names.
          line = line.replace(".%s_" % p, ".")
        # Exclude the original test name.
        filters.append(line)
        if line[-2:] != ".*":
          # List all possible prefixes if line doesn't end with ".*".
          for p in test_prefixes:
            filters.append(line.replace(".", ".%s_" % p))
    # Get rid of duplicates.
    filters = set(filters)
    gtest_filter = self._gtest_filter
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

  def SetupLdPath(self, requires_build_dir):
    if requires_build_dir:
      self._EnsureBuildDirFound()
    elif not self._options.build_dir:
      return

    # Append build_dir to LD_LIBRARY_PATH so external libraries can be loaded.
    if (os.getenv("LD_LIBRARY_PATH")):
      os.putenv("LD_LIBRARY_PATH", "%s:%s" % (os.getenv("LD_LIBRARY_PATH"),
                                              self._options.build_dir))
    else:
      os.putenv("LD_LIBRARY_PATH", self._options.build_dir)

  def SimpleTest(self, module, name, valgrind_test_args=None, cmd_args=None):
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, name, valgrind_test_args)
    self._AppendGtestFilter(tool, name, cmd)
    if cmd_args:
      cmd.extend(cmd_args)

    self.SetupLdPath(True)
    return tool.Run(cmd, module)

  def RunCmdLine(self):
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, None, self._args)
    self.SetupLdPath(False)
    return tool.Run(cmd, None)

  def TestAsh(self):
    return self.SimpleTest("ash", "aura_shell_unittests")

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests")

  def TestContent(self):
    return self.SimpleTest("content", "content_unittests")

  def TestCourgette(self):
    return self.SimpleTest("courgette", "courgette_unittests")

  def TestCrypto(self):
    return self.SimpleTest("crypto", "crypto_unittests")

  def TestGfx(self):
    return self.SimpleTest("chrome", "gfx_unittests")

  def TestGPU(self):
    return self.SimpleTest("gpu", "gpu_unittests")

  def TestGURL(self):
    return self.SimpleTest("chrome", "googleurl_unittests")

  def TestIpc(self):
    return self.SimpleTest("ipc", "ipc_tests",
                           valgrind_test_args=["--trace_children"])

  def TestJingle(self):
    return self.SimpleTest("chrome", "jingle_unittests")

  def TestMedia(self):
    return self.SimpleTest("chrome", "media_unittests")

  def TestNet(self):
    return self.SimpleTest("net", "net_unittests")

  def TestPPAPI(self):
    return self.SimpleTest("chrome", "ppapi_unittests")

  def TestPrinting(self):
    return self.SimpleTest("chrome", "printing_unittests")

  def TestRemoting(self):
    return self.SimpleTest("chrome", "remoting_unittests",
                           cmd_args=[
                               "--ui-test-action-timeout=60000",
                               "--ui-test-action-max-timeout=150000"])

  def TestSql(self):
    return self.SimpleTest("chrome", "sql_unittests")

  def TestSync(self):
    return self.SimpleTest("chrome", "sync_unit_tests")

  def TestTestShell(self):
    return self.SimpleTest("webkit", "test_shell_tests")

  def TestUnit(self):
    # http://crbug.com/51716
    # Disabling all unit tests
    # Problems reappeared after r119922
    if common.IsMac() and (self._options.valgrind_tool == "memcheck"):
      logging.warning("unit_tests are disabled for memcheck on MacOS.")
      return 0;
    return self.SimpleTest("chrome", "unit_tests")

  def TestUIUnit(self):
    return self.SimpleTest("chrome", "ui_unittests")

  def TestViews(self):
    return self.SimpleTest("views", "views_unittests")

  # Valgrind timeouts are in seconds.
  UI_VALGRIND_ARGS = ["--timeout=14400", "--trace_children", "--indirect"]
  # UI test timeouts are in milliseconds.
  UI_TEST_ARGS = ["--ui-test-action-timeout=60000",
                  "--ui-test-action-max-timeout=150000"]

  def TestAutomatedUI(self):
    return self.SimpleTest("chrome", "automated_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestBrowser(self):
    return self.SimpleTest("chrome", "browser_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestInteractiveUI(self):
    return self.SimpleTest("chrome", "interactive_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestReliability(self):
    script_dir = path_utils.ScriptDir()
    url_list_file = os.path.join(script_dir, "reliability", "url_list.txt")
    return self.SimpleTest("chrome", "reliability_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(self.UI_TEST_ARGS +
                                     ["--list=%s" % url_list_file]))

  def TestSafeBrowsing(self):
    return self.SimpleTest("chrome", "safe_browsing_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--ui-test-action-max-timeout=450000"]))

  def TestSyncIntegration(self):
    return self.SimpleTest("chrome", "sync_integration_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--ui-test-action-max-timeout=450000"]))

  def TestUI(self):
    return self.SimpleTest("chrome", "ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestLayoutChunk(self, chunk_num, chunk_size):
    # Run tests [chunk_num*chunk_size .. (chunk_num+1)*chunk_size) from the
    # list of tests.  Wrap around to beginning of list at end.
    # If chunk_size is zero, run all tests in the list once.
    # If a text file is given as argument, it is used as the list of tests.
    #
    # Build the ginormous commandline in 'cmd'.
    # It's going to be roughly
    #  python valgrind_test.py ... python run_webkit_tests.py ...
    # but we'll use the --indirect flag to valgrind_test.py
    # to avoid valgrinding python.
    # Start by building the valgrind_test.py commandline.
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool)
    cmd.append("--trace_children")
    cmd.append("--indirect_webkit_layout")
    cmd.append("--ignore_exit_code")
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
    script_cmd = ["python", script, "-v",
                  "--run-singly",  # run a separate DumpRenderTree for each test
                  "--experimental-fully-parallel",
                  "--time-out-ms=200000",
                  "--noshow-results",
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
    self._AppendGtestFilter(tool, "layout", script_cmd)
    # Now run script_cmd with the wrapper in cmd
    cmd.extend(["--"])
    cmd.extend(script_cmd)
    return tool.Run(cmd, "layout")

  def TestLayout(self):
    # A "chunk file" is maintained in the local directory so that each test
    # runs a slice of the layout tests of size chunk_size that increments with
    # each run.  Since tests can be added and removed from the layout tests at
    # any time, this is not going to give exact coverage, but it will allow us
    # to continuously run small slices of the layout tests under valgrind rather
    # than having to run all of them in one shot.
    chunk_size = self._options.num_tests
    if (chunk_size == 0):
      return self.TestLayoutChunk(0, 0)
    chunk_num = 0
    chunk_file = os.path.join("valgrind_layout_chunk.txt")
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

  # The known list of tests.
  # Recognise the original abbreviations as well as full executable names.
  _test_list = {
    "cmdline" : RunCmdLine,
    "ash": TestAsh,              "aura_shell_unittests": TestAsh,
    "automated_ui" : TestAutomatedUI,
    "base": TestBase,            "base_unittests": TestBase,
    "browser": TestBrowser,      "browser_tests": TestBrowser,
    "crypto": TestCrypto,        "crypto_unittests": TestCrypto,
    "googleurl": TestGURL,       "googleurl_unittests": TestGURL,
    "content": TestContent,      "content_unittests": TestContent,
    "courgette": TestCourgette,  "courgette_unittests": TestCourgette,
    "ipc": TestIpc,              "ipc_tests": TestIpc,
    "interactive_ui": TestInteractiveUI,
    "layout": TestLayout,        "layout_tests": TestLayout,
    "webkit": TestLayout,
    "media": TestMedia,          "media_unittests": TestMedia,
    "net": TestNet,              "net_unittests": TestNet,
    "jingle": TestJingle,        "jingle_unittests": TestJingle,
    "ppapi": TestPPAPI,          "ppapi_unittests": TestPPAPI,
    "printing": TestPrinting,    "printing_unittests": TestPrinting,
    "reliability": TestReliability, "reliability_tests": TestReliability,
    "remoting": TestRemoting,    "remoting_unittests": TestRemoting,
    "safe_browsing": TestSafeBrowsing, "safe_browsing_tests": TestSafeBrowsing,
    "sync": TestSync,            "sync_unit_tests": TestSync,
    "sync_integration_tests": TestSyncIntegration,
    "sync_integration": TestSyncIntegration,
    "test_shell": TestTestShell, "test_shell_tests": TestTestShell,
    "ui": TestUI,                "ui_tests": TestUI,
    "unit": TestUnit,            "unit_tests": TestUnit,
    "sql": TestSql,              "sql_unittests": TestSql,
    "ui_unit": TestUIUnit,       "ui_unittests": TestUIUnit,
    "gfx": TestGfx,              "gfx_unittests": TestGfx,
    "gpu": TestGPU,              "gpu_unittests": TestGPU,
    "views": TestViews,          "views_unittests": TestViews,
  }


def _main():
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()
  parser.add_option("-b", "--build_dir",
                    help="the location of the compiler output")
  parser.add_option("-t", "--test", action="append", default=[],
                    help="which test to run, supports test:gtest_filter format "
                         "as well.")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  parser.add_option("", "--tool", dest="valgrind_tool", default="memcheck",
                    help="specify a valgrind tool to run the tests under")
  parser.add_option("", "--tool_flags", dest="valgrind_tool_flags", default="",
                    help="specify custom flags for the selected valgrind tool")
  parser.add_option("", "--keep_logs", action="store_true", default=False,
                    help="store memory tool logs in the <tool>.logs directory "
                         "instead of /tmp.\nThis can be useful for tool "
                         "developers/maintainers.\nPlease note that the <tool>"
                         ".logs directory will be clobbered on tool startup.")
  parser.add_option("-n", "--num_tests", default=1500, type="int",
                    help="for layout tests: # of subtests per run.  0 for all.")

  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error("--test not specified")

  if len(options.test) != 1 and options.gtest_filter:
    parser.error("--gtest_filter and multiple tests don't make sense together")

  for t in options.test:
    tests = ChromeTests(options, args, t)
    ret = tests.Run()
    if ret: return ret
  return 0


if __name__ == "__main__":
  sys.exit(_main())
