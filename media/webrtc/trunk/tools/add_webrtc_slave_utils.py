#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

import os


def main():
  """
  Simple script for adding an import of the WebRTC slave_utils module for the
  buildbot slaves to the Chromium buildbot.tac file.
  Using this script, we don't need to maintain our own version of the slave
  scripts and can automatically stay up to date with their changes.
  It will add a comment and the import at the end of the file, if it's not
  already present.

  This script should be invoked as a hooks step in the DEPS file, like this:
  hooks = [
   {
      # Update slave buildbot.tac to include WebRTC slave_utils import.
      "pattern": ".",
      "action": ["python", "tools/add_webrtc_slave_utils.py"],
    },
  ]
  """
  SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
  TARGET_FILE = os.path.join(SCRIPT_PATH,
                             'continuous_build/build/slave/buildbot.tac')
  COMMENT_LINE = '# Load WebRTC custom slave script.\n'
  IMPORT_LINE = 'from webrtc_buildbot import slave_utils\n'

  file = open(TARGET_FILE, 'r')
  if file.read().find(IMPORT_LINE) == -1:
    print 'Patching %s with WebRTC imports.' % TARGET_FILE
    file.close()
    file = open(TARGET_FILE, 'a')
    file.write(COMMENT_LINE)
    file.write(IMPORT_LINE)
  file.close()

if __name__ == '__main__':
  main()
