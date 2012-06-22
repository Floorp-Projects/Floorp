#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Connects all URLs with their respective handlers."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

from google.appengine.ext.webapp import template
import webapp2

import add_build_status_data
import add_coverage_data
import dashboard
import lkgr_page

app = webapp2.WSGIApplication([('/', dashboard.ShowDashboard),
                               ('/lkgr', lkgr_page.ShowLkgr),
                               ('/add_coverage_data',
                                add_coverage_data.AddCoverageData),
                               ('/add_build_status_data',
                                add_build_status_data.AddBuildStatusData)],
                              debug=True)