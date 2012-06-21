#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements the quality tracker dashboard and reporting facilities."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

from google.appengine.ext.webapp import template
import webapp2

import load_build_status
import load_coverage


class ShowDashboard(webapp2.RequestHandler):
  """Shows the dashboard page.

     The page is shown by grabbing data we have stored previously
     in the App Engine database using the AddCoverageData handler.
  """

  def get(self):
    build_status_loader = load_build_status.BuildStatusLoader()
    build_status_data = build_status_loader.load_build_status_data()
    last_updated_at = build_status_loader.load_last_modified_at()
    if last_updated_at is None:
      self._show_error_page("No data has yet been uploaded to the dashboard.")
      return

    last_updated_at = last_updated_at.strftime("%Y-%m-%d %H:%M")
    lkgr = build_status_loader.compute_lkgr()

    coverage_loader = load_coverage.CoverageDataLoader()
    small_medium_coverage_json_data = (
        coverage_loader.load_coverage_json_data('small_medium_tests'))
    large_coverage_json_data = (
        coverage_loader.load_coverage_json_data('large_tests'))

    page_template_filename = 'templates/dashboard_template.html'
    self.response.write(template.render(page_template_filename, vars()))

  def _show_error_page(self, error_message):
    self.response.write('<html><body>%s</body></html>' % error_message)

