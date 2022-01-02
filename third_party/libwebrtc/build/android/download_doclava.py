#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Minimal tool to download doclava from Google storage when building for
Android."""

import os
import subprocess
import sys


def main():
  # Some Windows bots inadvertently have third_party/android_sdk installed,
  # but are unable to run download_from_google_storage because depot_tools
  # is not in their path, so avoid failure and bail.
  if sys.platform == 'win32':
    return 0
  subprocess.check_call([
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket', 'chromium-doclava',
      '--extract',
      '-s',
      os.path.join(os.path.dirname(__file__), '..', '..', 'buildtools',
                   'android', 'doclava.tar.gz.sha1')])
  return 0

if __name__ == '__main__':
  sys.exit(main())
