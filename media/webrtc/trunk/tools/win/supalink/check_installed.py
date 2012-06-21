#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

def main():
  if sys.platform != 'win32':
    sys.stdout.write('0')
    return 0

  import _winreg

  try:
    val = _winreg.QueryValue(_winreg.HKEY_CURRENT_USER,
                             'Software\\Chromium\\supalink_installed')
    if os.path.exists(val):
      # Apparently gyp thinks this means there was an error?
      #sys.stderr.write('Supalink enabled.\n')
      sys.stdout.write('1')
      return 0
  except WindowsError:
    pass

  sys.stdout.write('0')
  return 0


if __name__ == '__main__':
  sys.exit(main())
