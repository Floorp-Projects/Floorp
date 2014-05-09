#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import subprocess
import os.path

def main():
  return subprocess.call([sys.executable, "../webrtc/trunk/build/dir_exists.py"] + sys.argv[1:])

if __name__ == '__main__':
  sys.exit(main())
