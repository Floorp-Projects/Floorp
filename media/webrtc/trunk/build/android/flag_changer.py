# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Location where chrome reads command line flags from
CHROME_COMMAND_FILE = '/data/local/chrome-command-line'


class FlagChanger(object):
  """Temporarily changes the flags Chrome runs with."""

  def __init__(self, android_cmd):
    self._android_cmd = android_cmd
    self._old_flags = None

  def Set(self, flags, append=False):
    """Sets the command line flags used when chrome is started.

    Args:
      flags: A list of flags to set, eg. ['--single-process'].
      append: Whether to append to existing flags or overwrite them.
    """
    if flags:
      assert flags[0] != 'chrome'

    if not self._old_flags:
      self._old_flags = self._android_cmd.GetFileContents(CHROME_COMMAND_FILE)
      if self._old_flags:
        self._old_flags = self._old_flags[0].strip()

    if append and self._old_flags:
      # Avoid appending flags that are already present.
      new_flags = filter(lambda flag: self._old_flags.find(flag) == -1, flags)
      self._android_cmd.SetFileContents(CHROME_COMMAND_FILE,
                                        self._old_flags + ' ' +
                                        ' '.join(new_flags))
    else:
      self._android_cmd.SetFileContents(CHROME_COMMAND_FILE,
                                        'chrome ' + ' '.join(flags))

  def Restore(self):
    """Restores the flags to their original state."""
    if self._old_flags == None:
      return  # Set() was never called.
    elif self._old_flags:
      self._android_cmd.SetFileContents(CHROME_COMMAND_FILE, self._old_flags)
    else:
      self._android_cmd.RunShellCommand('rm ' + CHROME_COMMAND_FILE)
