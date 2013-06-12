#!/usr/bin/env python
#
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


"""Builds the WebRTC Video Demo for Android.

This script is designed as an annotation script to be run by a Chrome Buildbot.

PREREQUISITES: You must have built WebRTC with the right environment set (the
one you get when sourcing build/android/envsetup.sh) before building with this
script.

NOTICE: To build successfully, you have to have Ant installed and have your
.gclient file setup with the target_os = ["android"] variable appended to the
last line of your WebRTC solution, so it looks like this:
];target_os = ["android"]
Then run 'gclient sync' to sync out the required Android SDK and NDK files into
the third_party directory.
If you want to get additional platform-specific dependencies in the same
checkout, add them to the list too, e.g. target_os = ["android", "unix"].
"""

import optparse
import os
import subprocess
import sys

_CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
_ROOT_DIR = os.path.abspath(os.path.join(_CURRENT_DIR, '..', '..', '..', '..'))
_ANDROID_ENV_SCRIPT = os.path.join(_ROOT_DIR, 'build', 'android', 'envsetup.sh')

def main():
  parser = optparse.OptionParser('usage: %prog -t <target>')

  parser.add_option('-t', '--target', default='debug',
                    help='Compile target (debug/release). Default: %default')
  # Build and factory properties are currently unused but are required to avoid
  # errors when the script is executed by the buildbots.
  parser.add_option('--build-properties', help='Build properties (unused)')
  parser.add_option('--factory-properties', help='Factory properties (unused)')
  options, _args = parser.parse_args()

  def RunInAndroidEnv(cmd):
    return 'source %s && %s' % (_ANDROID_ENV_SCRIPT, cmd)

  print '@@@BUILD_STEP ndk-build@@@'
  cmd = RunInAndroidEnv('ndk-build')
  print cmd
  try:
    subprocess.check_call(cmd, cwd=_CURRENT_DIR, executable='/bin/bash',
                          shell=True)
  except subprocess.CalledProcessError as e:
    print 'NDK build failed: %s' % e
    print '@@@STEP_FAILURE@@@'
    return 1

  print '@@@BUILD_STEP ant-build@@@'
  cmd = RunInAndroidEnv('ant %s' % options.target.lower())
  print cmd
  try:
    subprocess.check_call(cmd, cwd=_CURRENT_DIR, executable='/bin/bash',
                          shell=True)
  except subprocess.CalledProcessError as e:
    print 'Ant build failed: %s' % e
    print '@@@STEP_FAILURE@@@'
    return 2

  print 'WebRTC Demo build completed.'
  return 0

if __name__ == '__main__':
  sys.exit(main())
