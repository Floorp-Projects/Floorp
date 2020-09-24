# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper for subprocess to make calling shell commands easier."""


import logging
import subprocess


def RunCmd(args, cwd=None):
  """Opens a subprocess to execute a program and returns its return value.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.

  Returns:
    Return code from the command execution.
  """
  logging.info(str(args) + ' ' + (cwd or ''))
  p = subprocess.Popen(args=args, cwd=cwd)
  return p.wait()


def GetCmdOutput(args, cwd=None, shell=False):
  """Open a subprocess to execute a program and returns its output.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.
    shell: Whether to execute args as a shell command.

  Returns:
    Captures and returns the command's stdout.
    Prints the command's stderr to logger (which defaults to stdout).
  """
  logging.info(str(args) + ' ' + (cwd or ''))
  p = subprocess.Popen(args=args, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, shell=shell)
  stdout, stderr = p.communicate()
  if stderr:
    logging.critical(stderr)
  logging.info(stdout[:4096])  # Truncate output longer than 4k.
  return stdout
