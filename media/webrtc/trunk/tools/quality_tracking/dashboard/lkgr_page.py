#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements the LKGR page."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

import webapp2

import load_build_status

class ShowLkgr(webapp2.RequestHandler):
  """This handler shows the LKGR in the simplest possible way.

     The page is intended to be used by automated tools.
  """
  def get(self):
    build_status_loader = load_build_status.BuildStatusLoader()

    lkgr = build_status_loader.compute_lkgr()
    if lkgr is None:
      self.response.out.write('No data has been uploaded to the dashboard.')
    else:
      self.response.out.write(lkgr)
