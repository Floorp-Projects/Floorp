# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running the test under AddressSanitizer."""

import datetime
import logging
import os
import re

import common
import path_utils
import suppressions


class ASanWrapper(object):
  def __init__(self, supp_files):
    self._timeout = 1200

  def PutEnvAndLog(self, env_name, env_value):
    """Sets the env var |env_name| to |env_value| and writes to logging.info.
    """
    os.putenv(env_name, env_value)
    logging.info('export %s=%s', env_name, env_value)

  def Execute(self):
    """Executes the app to be tested."""
    logging.info('starting execution...')
    proc = self._args
    self.PutEnvAndLog('G_SLICE', 'always-malloc')
    self.PutEnvAndLog('NSS_DISABLE_ARENA_FREE_LIST', '1')
    self.PutEnvAndLog('NSS_DISABLE_UNLOAD', '1')
    self.PutEnvAndLog('GTEST_DEATH_TEST_USE_FORK', '1')
    return common.RunSubprocess(proc, self._timeout)

  def Main(self, args):
    self._args = args
    start = datetime.datetime.now()
    retcode = -1
    retcode = self.Execute()
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds %= 3600
    minutes = seconds / 60
    seconds %= 60
    logging.info('elapsed time: %02d:%02d:%02d', hours, minutes, seconds)
    logging.info('For more information on the AddressSanitizer bot see '
                 'http://dev.chromium.org/developers/testing/'
                 'addresssanitizer')
    return retcode


def RunTool(args, supp_files, module):
  tool = ASanWrapper(supp_files)
  return tool.Main(args[1:])
