#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Contains a customized GClient class for WebRTC use.

It is essential that this class is imported after the chromium_commands so it
can overwrite its registration of the 'gclient' command.
This should by done by adding:

from webrtc_buildbot import slave_utils

into the buildbot.tac file of the try slave.
"""

from slave import chromium_commands

try:
  # Buildbot 0.8.x
  # Unable to import 'XX'
  # pylint: disable=F0401
  from buildslave.commands.registry import commandRegistry
except ImportError:
  # Buildbot 0.7.12
  # Unable to import 'XX'
  # pylint: disable=E0611,E1101,F0401
  from buildbot.slave.registry import registerSlaveCommand

class GClient(chromium_commands.GClient):
  def doPatch(self, res):
    # For some unknown reason, the patch tuple recieved only have two items:
    # the patch level and the actual diff contents. The third optional root
    # argument that tells which base directory to use for the patch is not
    # included. Since we need this to be able to patch properly in WebRTC, we'll
    # just create a new tuple with this added.
    self.patch = (self.patch[0], self.patch[1], 'trunk')
    return chromium_commands.GClient.doPatch(self, res)


def RegisterGclient():
  try:
    # This version should work on BB 8.3

    # We run this code in a try because it fails with an assertion if
    # the module is loaded twice.
    commandRegistry['gclient'] = 'webrtc_buildbot.slave_utils.GClient'
    return
  except (AssertionError, NameError):
    pass

  try:
    # This version should work on BB 7.12
    # We run this code in a try because it fails with an assertion if
    # the module is loaded twice.
    registerSlaveCommand('gclient', GClient, commands.command_version)
  except (AssertionError, NameError):
    pass

RegisterGclient()
