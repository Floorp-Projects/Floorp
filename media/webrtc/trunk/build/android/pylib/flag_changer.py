# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import constants
import traceback
import warnings


# Location where chrome reads command line flags from
CHROME_COMMAND_FILE = constants.TEST_EXECUTABLE_DIR + '/chrome-command-line'

class FlagChanger(object):
  """Changes the flags Chrome runs with.

  There are two different use cases for this file:
  * Flags are permanently set by calling Set().
  * Flags can be temporarily set for a particular set of unit tests.  These
    tests should call Restore() to revert the flags to their original state
    once the tests have completed.
  """

  def __init__(self, android_cmd):
    self._android_cmd = android_cmd

    # Save the original flags.
    self._orig_line = self._android_cmd.GetFileContents(CHROME_COMMAND_FILE)
    if self._orig_line:
      self._orig_line = self._orig_line[0].strip()

    # Parse out the flags into a list to facilitate adding and removing flags.
    self._current_flags = self._TokenizeFlags(self._orig_line)

  def Get(self):
    """Returns list of current flags."""
    return self._current_flags

  def Set(self, flags):
    """Replaces all flags on the current command line with the flags given.

    Args:
      flags: A list of flags to set, eg. ['--single-process'].
    """
    if flags:
      assert flags[0] != 'chrome'

    self._current_flags = flags
    self._UpdateCommandLineFile()

  def AddFlags(self, flags):
    """Appends flags to the command line if they aren't already there.

    Args:
      flags: A list of flags to add on, eg. ['--single-process'].
    """
    if flags:
      assert flags[0] != 'chrome'

    # Avoid appending flags that are already present.
    for flag in flags:
      if flag not in self._current_flags:
        self._current_flags.append(flag)
    self._UpdateCommandLineFile()

  def RemoveFlags(self, flags):
    """Removes flags from the command line, if they exist.

    Args:
      flags: A list of flags to remove, eg. ['--single-process'].  Note that we
             expect a complete match when removing flags; if you want to remove
             a switch with a value, you must use the exact string used to add
             it in the first place.
    """
    if flags:
      assert flags[0] != 'chrome'

    for flag in flags:
      if flag in self._current_flags:
        self._current_flags.remove(flag)
    self._UpdateCommandLineFile()

  def Restore(self):
    """Restores the flags to their original state."""
    self._current_flags = self._TokenizeFlags(self._orig_line)
    self._UpdateCommandLineFile()

  def _UpdateCommandLineFile(self):
    """Writes out the command line to the file, or removes it if empty."""
    print "Current flags: ", self._current_flags

    if self._current_flags:
      self._android_cmd.SetFileContents(CHROME_COMMAND_FILE,
                                        'chrome ' +
                                        ' '.join(self._current_flags))
    else:
      self._android_cmd.RunShellCommand('rm ' + CHROME_COMMAND_FILE)

  def _TokenizeFlags(self, line):
    """Changes the string containing the command line into a list of flags.

    Follows similar logic to CommandLine.java::tokenizeQuotedArguments:
    * Flags are split using whitespace, unless the whitespace is within a
      pair of quotation marks.
    * Unlike the Java version, we keep the quotation marks around switch
      values since we need them to re-create the file when new flags are
      appended.

    Args:
      line: A string containing the entire command line.  The first token is
            assumed to be the program name.
    """
    if not line:
      return []

    tokenized_flags = []
    current_flag = ""
    within_quotations = False

    # Move through the string character by character and build up each flag
    # along the way.
    for c in line.strip():
      if c is '"':
        if len(current_flag) > 0 and current_flag[-1] == '\\':
          # Last char was a backslash; pop it, and treat this " as a literal.
          current_flag = current_flag[0:-1] + '"'
        else:
          within_quotations = not within_quotations
          current_flag += c
      elif not within_quotations and (c is ' ' or c is '\t'):
        if current_flag is not "":
          tokenized_flags.append(current_flag)
          current_flag = ""
      else:
        current_flag += c

    # Tack on the last flag.
    if not current_flag:
      if within_quotations:
        warnings.warn("Unterminated quoted string: " + current_flag)
    else:
      tokenized_flags.append(current_flag)

    # Return everything but the program name.
    return tokenized_flags[1:]
