# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Classes in this file define additional actions that need to be taken to run a
test under some kind of runtime error detection tool.

The interface is intended to be used as follows.

1. For tests that simply run a native process (i.e. no activity is spawned):

Call tool.CopyFiles().
Prepend test command line with tool.GetTestWrapper().

2. For tests that spawn an activity:

Call tool.CopyFiles().
Call tool.SetupEnvironment().
Run the test as usual.
Call tool.CleanUpEnvironment().
"""

import os.path
import sys

from run_tests_helper import CHROME_DIR


class BaseTool(object):
  """A tool that does nothing."""

  def __init__(self, *args, **kwargs):
    pass

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ''

  def CopyFiles(self):
    """Copies tool-specific files to the device, create directories, etc."""
    pass

  def SetupEnvironment(self):
    """Sets up the system environment for a test.

    This is a good place to set system properties.
    """
    pass

  def CleanUpEnvironment(self):
    """Cleans up environment."""
    pass

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 1.0

  def NeedsDebugInfo(self):
    """Whether this tool requires debug info.

    Returns True if this tool can not work with stripped binaries.
    """
    return False


class ValgrindTool(BaseTool):
  """Base abstract class for Valgrind tools."""

  VG_DIR = '/data/local/tmp/valgrind'
  VGLOGS_DIR = '/data/local/tmp/vglogs'

  def __init__(self, adb, renderer=False):
    self.adb = adb
    if renderer:
      # exactly 31 chars, SystemProperties::PROP_NAME_MAX
      self.wrap_property = 'wrap.com.android.chrome:sandbox'
    else:
      self.wrap_property = 'wrap.com.android.chrome'

  def CopyFiles(self):
    """Copies Valgrind tools to the device."""
    self.adb.RunShellCommand('rm -r %s; mkdir %s' %
                             (ValgrindTool.VG_DIR, ValgrindTool.VG_DIR))
    self.adb.RunShellCommand('rm -r %s; mkdir %s' %
                             (ValgrindTool.VGLOGS_DIR, ValgrindTool.VGLOGS_DIR))
    files = self.GetFilesForTool()
    for f in files:
      self.adb.PushIfNeeded(os.path.join(CHROME_DIR, f),
                            os.path.join(ValgrindTool.VG_DIR,
                                         os.path.basename(f)))

  def SetupEnvironment(self):
    """Sets up device environment."""
    self.adb.RunShellCommand('chmod 777 /data/local/tmp')
    self.adb.RunShellCommand('setprop %s "logwrapper %s"' % (
        self.wrap_property, self.GetTestWrapper()))
    self.adb.RunShellCommand('setprop chrome.timeout_scale %f' % (
        self.GetTimeoutScale()))

  def CleanUpEnvironment(self):
    """Cleans up device environment."""
    self.adb.RunShellCommand('setprop %s ""' % (self.wrap_property,))
    self.adb.RunShellCommand('setprop chrome.timeout_scale ""')

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    raise NotImplementedError()

  def NeedsDebugInfo(self):
    """Whether this tool requires debug info.

    Returns True if this tool can not work with stripped binaries.
    """
    return True


class MemcheckTool(ValgrindTool):
  """Memcheck tool."""

  def __init__(self, adb, renderer=False):
    super(MemcheckTool, self).__init__(adb, renderer)

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    return ['tools/valgrind/android/vg-chrome-wrapper.sh',
            'tools/valgrind/memcheck/suppressions.txt',
            'tools/valgrind/memcheck/suppressions_android.txt']

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ValgrindTool.VG_DIR + '/' + 'vg-chrome-wrapper.sh'

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 30


class TSanTool(ValgrindTool):
  """ThreadSanitizer tool. See http://code.google.com/p/data-race-test ."""

  def __init__(self, adb, renderer=False):
    super(TSanTool, self).__init__(adb, renderer)

  def GetFilesForTool(self):
    """Returns a list of file names for the tool."""
    return ['tools/valgrind/android/vg-chrome-wrapper-tsan.sh',
            'tools/valgrind/tsan/suppressions.txt',
            'tools/valgrind/tsan/suppressions_android.txt',
            'tools/valgrind/tsan/ignores.txt']

  def GetTestWrapper(self):
    """Returns a string that is to be prepended to the test command line."""
    return ValgrindTool.VG_DIR + '/' + 'vg-chrome-wrapper-tsan.sh'

  def GetTimeoutScale(self):
    """Returns a multiplier that should be applied to timeout values."""
    return 30


TOOL_REGISTRY = {
  'memcheck': lambda x: MemcheckTool(x, False),
  'memcheck-renderer': lambda x: MemcheckTool(x, True),
  'tsan': lambda x: TSanTool(x, False),
  'tsan-renderer': lambda x: TSanTool(x, True)
}


def CreateTool(tool_name, adb):
  """Creates a tool with the specified tool name.

  Args:
    tool_name: Name of the tool to create.
    adb: ADB interface the tool will use.
  """
  if not tool_name:
    return BaseTool()

  ctor = TOOL_REGISTRY.get(tool_name)
  if ctor:
    return ctor(adb)
  else:
    print 'Unknown tool %s, available tools: %s' % (
      tool_name, ', '.join(sorted(TOOL_REGISTRY.keys())))
    sys.exit(1)
