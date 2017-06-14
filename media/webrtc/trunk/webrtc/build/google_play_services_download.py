#!/usr/bin/env python
# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to download the Google Play Services SDK without its license prompt.

This script needs to run after Chromium has been cloned and the link to //build
has been created.
"""

import os
import subprocess
import sys


ROOT_DIR = os.path.abspath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), os.pardir, os.pardir))
CR_DIR = os.path.join(ROOT_DIR, 'chromium')


def main():
  # Workaround to avoid license prompt for Google Play Services SDK
  # See https://bugs.webrtc.org/5578 for more details.
  play_services_script = os.path.join(CR_DIR, 'src', 'build', 'android',
                                      'play_services', 'update.py')
  if os.path.isfile(play_services_script):
    env = os.environ.copy()

    # Fake we're a buildbot to avoid the Google Play Services license prompt.
    env['CHROME_HEADLESS'] = '1'
    subprocess.check_call([sys.executable, play_services_script, 'download'],
                          env=env)
  else:
    print 'Cannot find %s, skipping.' % play_services_script


if __name__ == '__main__':
  sys.exit(main())
