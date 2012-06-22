# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def CheckChange(input_api, output_api):
  """Checks the TSan suppressions files for bad suppressions."""

  # TODO(timurrrr): find out how to do relative imports
  # and remove this ugly hack. Also, the CheckChange function won't be needed.
  tools_vg_path = os.path.join(input_api.PresubmitLocalPath(), '..')
  sys.path.append(tools_vg_path)
  import suppressions

  return suppressions.PresubmitCheck(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

def GetPreferredTrySlaves():
  return ['linux_tsan']
