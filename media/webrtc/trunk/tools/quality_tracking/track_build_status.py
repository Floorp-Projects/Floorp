#!/usr/bin/env python
#-*- coding: utf-8 -*-
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script checks the current build status on the master and submits
   it to the dashboard. It is adapted to build bot version 0.7.12.
"""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import httplib

import constants
import dashboard_connection
import tgrid_parser


class FailedToGetStatusFromMaster(Exception):
  pass


def _download_and_parse_build_status():
  connection = httplib.HTTPConnection(constants.BUILD_MASTER_SERVER)
  connection.request('GET', constants.BUILD_MASTER_TRANSPOSED_GRID_URL)
  response = connection.getresponse()

  if response.status != 200:
    raise FailedToGetStatusFromMaster(('Failed to get build status from master:'
                                       ' got status %d, reason %s.' %
                                       (response.status, response.reason)))

  full_response = response.read()
  connection.close()

  return tgrid_parser.parse_tgrid_page(full_response)


def _is_chrome_only_build(revision_to_bot_name):
  """Figures out if a revision-to-bot-name mapping represents a Chrome build.

     We assume here that Chrome revisions are always > 100000, whereas WebRTC
     revisions will not reach that number in the foreseeable future."""
  revision = int(revision_to_bot_name.split('--')[0])
  bot_name = revision_to_bot_name.split('--')[1]
  return bot_name == 'Chrome' and revision > 100000


def _filter_chrome_only_builds(bot_to_status_mapping):
  """Filters chrome-only builds from the system so LKGR doesn't get confused."""
  return dict((revision_to_bot_name, status)
              for revision_to_bot_name, status
              in bot_to_status_mapping.iteritems()
              if not _is_chrome_only_build(revision_to_bot_name))


def _main():
  dashboard = dashboard_connection.DashboardConnection(constants.CONSUMER_KEY)
  dashboard.read_required_files(constants.CONSUMER_SECRET_FILE,
                                constants.ACCESS_TOKEN_FILE)

  bot_to_status_mapping = _download_and_parse_build_status()
  bot_to_status_mapping = _filter_chrome_only_builds(bot_to_status_mapping)

  dashboard.send_post_request(constants.ADD_BUILD_STATUS_DATA_URL,
                              bot_to_status_mapping)


if __name__ == '__main__':
  _main()
