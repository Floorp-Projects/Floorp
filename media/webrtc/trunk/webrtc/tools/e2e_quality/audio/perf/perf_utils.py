# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copied from /src/chrome/test/pyautolib/pyauto_utils.py in Chromium.

import sys

def PrintPerfResult(graph_name, series_name, data_point, units,
                    show_on_waterfall=False):
  """Prints a line to stdout that is specially formatted for the perf bots.

  Args:
    graph_name: String name for the graph on which to plot the data.
    series_name: String name for the series (line on the graph) associated with
                 the data.  This is also the string displayed on the waterfall
                 if |show_on_waterfall| is True.
    data_point: Numeric data value to plot on the graph for the current build.
                This can be a single value or an array of values.  If an array,
                the graph will plot the average of the values, along with error
                bars.
    units: The string unit of measurement for the given |data_point|.
    show_on_waterfall: Whether or not to display this result directly on the
                       buildbot waterfall itself (in the buildbot step running
                       this test on the waterfall page, not the stdio page).
  """
  waterfall_indicator = ['', '*'][show_on_waterfall]
  print '%sRESULT %s: %s= %s %s' % (
      waterfall_indicator, graph_name, series_name,
      str(data_point).replace(' ', ''), units)
  sys.stdout.flush()