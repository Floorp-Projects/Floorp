#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Writes True if the argument is a directory."""

import os.path
import sys

def main():
  sys.stdout.write(str(os.path.isdir(sys.argv[1])))
  return 0

if __name__ == '__main__':
  sys.exit(main())
