# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions common to native, java and python test runners."""

import logging
import os


def GetExpectations(file_name):
  """Returns a list of test names in the |file_name| test expectations file."""
  if not file_name or not os.path.exists(file_name):
    return []
  return [x for x in [x.strip() for x in file(file_name).readlines()]
          if x and x[0] != '#']


def SetLogLevel(verbose_count):
  """Sets log level as |verbose_count|."""
  log_level = logging.WARNING  # Default.
  if verbose_count == 1:
    log_level = logging.INFO
  elif verbose_count >= 2:
    log_level = logging.DEBUG
  logging.getLogger().setLevel(log_level)
