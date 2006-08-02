#!c:/Python24/python.exe
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is standalone Firefox Windows performance test.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Annie Sullivan <annie.sullivan@gmail.com> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""Writes a report with the results of the Ts (startup) and Tp (page load) tests.

   The report contains the mean startup time for each profile and the standard
   deviation, the sum of page load times and the standard deviation, and a graph
   of each performance counter measured during the page load test.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import csv
import math
import matplotlib.mlab
import os
import pylab
import re
import time

import paths


def MakeArray(start, len, step):
  """Helper function to create an array for an axis to plot counter data.
  
  Args:
    start: The first value in the array
    len: The length of the array
    step: The difference between values in the array
  
  Returns:
    An array starting at start, with len values each step apart.
  """
  
  count = start
  end = start + (len * step)
  array = []
  while count < end:
    array.append(count)
    count += step
    
  return array


def GetPlottableData(counter_name, data):
  """Some counters should be displayed as a moving average, or
     may need other adjustment to be plotted.  This function
     makes adjustments to the data based on counter name.

  Args:
    counter_name: The name of the counter, i.e 'Working Set'
    data: The original data collected from the counter

  Returns:
    data array adjusted based on counter name.
  """

  if counter_name == '% Processor Time':
    # Use a moving average for % processor time
    return matplotlib.mlab.movavg(data, 5)
  if counter_name == 'Working Set' or counter_name == 'Private Bytes':
    # Change the scale from bytes to megabytes for working set
    return [float(x) / 1000000 for x in data]

  # No change for other counters
  return data


def GenerateReport(title, filename, configurations, ts_times, tp_times, tp_counters, tp_resolution):
  """ Generates a report file in html using the given data
  
  Args:
    title: Title of the report
    filename: Filename of the report, before the timestamp
    configurations: Array of strings, containing the name of
                    each configuration tested.
    ts_times: Array of arrays of ts startup times for each configuration.
    tp_times: Array of page load times for each configuration tested.
    tp_counters: Array of counter data for page load configurations
    
  Returns:
    filename of html report.
  """
  
  # Make sure the reports/ and reports/graphs/ directories exist
  graphs_subdir = os.path.join(paths.REPORTS_DIR, 'graphs')
  if not os.path.exists(graphs_subdir):
    os.makedirs(graphs_subdir) # Will create parent directories

  # Create html report file
  localtime = time.localtime()
  timestamp = int(time.mktime(localtime))
  report_filename = os.path.join(paths.REPORTS_DIR, filename + "_" + str(timestamp) + ".html")
  report = open(report_filename, 'w')
  report.write('<html><head><title>Performance Report for %s, %s</title></head>\n' %
               (title, time.strftime('%m-%d-%y')))
  report.write('<body>\n')
  report.write('<h1>%s, %s</h1>' % (title, time.strftime('%m-%d-%y')))
  
  # Write out TS data
  report.write('<p><h2>Startup Test (Ts) Results</h2>\n')
  report.write('<table border="1" cellpadding="5" cellspacing="0">\n')
  report.write('<tr>')
  report.write('<th>Profile Tested</th>')
  report.write('<th>Mean</th>')
  report.write('<th>Standard Deviation</th></tr>\n')
  ts_csv_filename =  os.path.join(paths.REPORTS_DIR, filename + "_" + str(timestamp) + '_ts.csv')
  ts_csv_file = open(ts_csv_filename, 'wb')
  ts_csv = csv.writer(ts_csv_file)
  for i in range (0, len(configurations)):
    # Calculate mean
    mean = 0
    for ts_time in ts_times[i]:
      mean += float(ts_time)
    mean = mean / len(ts_times[i])
    
    # Calculate standard deviation
    stdd = 0
    for ts_time in ts_times[i]:
      stdd += (float(ts_time) - mean) * (float(ts_time) - mean)
    stdd = stdd / len(ts_times[i])
    stdd = math.sqrt(stdd)
    
    report.write('<tr><td>%s</td><td>%f</td><td>%f</td></tr>\n' %
                 (configurations[i], mean, stdd))
    ts_csv.writerow([configurations[i], mean, stdd])
  report.write('</table></p>\n')
  ts_csv_file.close()
  
  # Write out TP data
  report.write('<p><h2>Page Load Test (Tp) Results</h2>\n')
  report.write('<table border="1" cellpadding="5" cellspacing="0">\n')
  report.write('<tr>')
  report.write('<th>Profile Tested</th>')
  report.write('<th>Sum of mean times</th>')
  report.write('<th>Sum of Standard Deviations</th></tr>\n')
  tp_csv_filename =  os.path.join(paths.REPORTS_DIR, filename + "_" + str(timestamp) + '_tp.csv')
  tp_csv_file = open(tp_csv_filename, 'wb')
  tp_csv = csv.writer(tp_csv_file)
  
  # Write out TP data
  for i in range (0, len(tp_times)):
    (tmp1, mean, tmp2, stdd) = tp_times[i].split()
    report.write('<tr><td>%s</td><td>%f</td><td>%f</td></tr>\n' %
                 (configurations[i], float(mean), float(stdd)))
    tp_csv.writerow([configurations[i], float(mean), float(stdd)])
  report.write('</table></p>\n')
  tp_csv_file.close()
  
  # Write out counter data from TP tests
  report.write('<p><h2>Performance Data</h2></p>\n')
  
  # Write out graph of performance for each counter
  colors = ['r-', 'g-', 'b-', 'y-', 'c-', 'm-']
  nonchar = re.compile('[\W]*')
  if len(tp_counters) > 0:
    counter_names = []
    for counter in tp_counters[0]:
      counter_names.append(counter)
    for counter_name in counter_names:

      # Create a new figure for this counter
      pylab.clf()

      # Label the figure, and the x/y axes
      pylab.title(counter_name)
      pylab.ylabel(counter_name)
      pylab.xlabel("Time")

      # Draw a line for each counter in a different color on the graph
      current_color = 0
      line_handles = [] # Save the handle of each line for the legend
      for count_data in tp_counters:
        data = GetPlottableData(counter_name, count_data[counter_name])
        times = MakeArray(0, len(data), tp_resolution)
        handle = pylab.plot(times, data, colors[current_color])
        line_handles.append(handle)
        current_color = (current_color + 1) % len(colors)

      # Draw a legend in the upper right corner
      legend = pylab.legend(line_handles, configurations, 'upper right')
      ltext = legend.get_texts()
      pylab.setp(ltext, fontsize='small')  # legend text is too large by default

      # Save the graph and link to it from html.
      image_name = os.path.join(graphs_subdir,
                                filename + "_" + str(timestamp) + nonchar.sub('', counter_name) + '.png')
      pylab.savefig(image_name)
      img_src = image_name[len(paths.REPORTS_DIR) : ]
      if img_src.startswith('\\'):
        img_src = img_src[1 : ]
      img_src = img_src.replace('\\', '/')
      report.write('<p><img src="%s" alt="%s"></p>\n' % (img_src, counter_name))
      
  report.write('</body></html>\n')
  
  return report_filename
