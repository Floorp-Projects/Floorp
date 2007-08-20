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
#   Alice Nodelman <anodelman@mozilla.com>
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

"""Runs extension performance tests.

   This file runs Ts (startup time) and Tp (page load time) tests
   for an extension with different profiles.  It was originally
   written for the Google Toolbar for Firefox; to make it work for
   another extension modify get_xpi.py.  To change the preferences
   that are set on the profiles that are tested, edit the arrays in
   the main function below.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import time
import yaml
import sys
import urllib 
import tempfile
import os
import string
import socket
socket.setdefaulttimeout(480)

import utils
import config
import post_file
import tp
import ts

def shortNames(name):
  if name == "tp_loadtime":
    return "tp_l"
  elif name == "tp_Percent Processor Time":
    return "tp_%cpu"
  elif name == "tp_Working Set":
    return "tp_memset"
  elif name == "tp_Private Bytes":
    return "tp_pbytes"
  else:
    return name

def process_Request(post):
  str = ""
  lines = post.split('\n')
  for line in lines:
    if line.find("RETURN:") > -1:
        str += line.split(":")[3] + ":" + shortNames(line.split(":")[1]) + ":" + line.split(":")[2] + '\n'
  return str

def test_file(filename):
  """Runs the Ts and Tp tests on the given config file and generates a report.
  
  Args:
    filename: the name of the file to run the tests on
  """
  
  test_configs = []
  test_names = []
  title = ''
  filename_prefix = ''
  testdate = ''
  
  # Read in the profile info from the YAML config file
  config_file = open(filename, 'r')
  yaml_config = yaml.load(config_file)
  config_file.close()
  for item in yaml_config:
    if item == 'title':
      title = yaml_config[item]
    elif item == 'filename':
      filename_prefix = yaml_config[item]
    elif item == 'testdate':
      testdate = yaml_config[item]
    else:
      new_config = [yaml_config[item]['preferences'],
                    yaml_config[item]['extensions'],
                    yaml_config[item]['firefox'],
                    yaml_config[item]['branch'],
                    yaml_config[item]['branchid'],
                    yaml_config[item]['profile_path'],
                    yaml_config[item]['env']]
      test_configs.append(new_config)
      test_names.append(item)
  config_file.close()

  print test_configs
  sys.stdout.flush()
  if (testdate != ''):
    date = int(time.mktime(time.strptime(testdate, '%a, %d %b %Y %H:%M:%S GMT')))
  else:
    date = int(time.time()) #TODO get this into own file
  print "using testdate: %d" % date
  print "actual date: %d" % int(time.time())
 

  # Run startup time test
  ts_times = ts.RunStartupTests(test_configs,
                                config.TS_NUM_RUNS)

  print "finished ts"
  sys.stdout.flush()
  for ts_set in ts_times:
    if len(ts_set) == 0:
	print "FAIL:no ts results, build failed to run:BAD BUILD"
	sys.exit(0)

  (res, r_strings, tp_times, tp_counters) = tp.RunPltTests(test_configs,
                                           config.TP_NUM_CYCLES,
                                           config.COUNTERS,
                                           config.TP_RESOLUTION)

  print "finished tp"
  sys.stdout.flush()

  if not res:
    print "FAIL:tp did not run to completion"
    print "FAIL:" + r_strings[0]
    sys.exit(0)

  #TODO: place this in its own file
  #send results to the graph server
  # each line of the string is of the format i;page_name;median;mean;min;max;time vals\n
  tbox = title
  url_format = "http://%s/%s"
  link_format= "<a href = \"%s\">%s</a>"
  #value, testname, tbox, timeval, date, branch, branchid, type, data
  result_format = "%.2f,%s,%s,%d,%d,%s,%s,%s,%s,\n"
  result_format2 = "%.2f,%s,%s,%d,%d,%s,%s,%s,\n"
  filename = tempfile.mktemp()
  tmpf = open(filename, "w")

  testname = "ts"
  print "formating results for: ts"
  print "# of values: %d" % len(ts_times)
  for index in range(len(ts_times)):
    i = 0
    for tstime in ts_times[index]:
      tmpf.write(result_format % (float(tstime), testname, tbox, i, date, test_configs[index][3], test_configs[index][4], "discrete", "ms"))
      i = i+1

  testname = "tp"
  for index in range(len(r_strings)):
    r_strings[index].strip('\n')
    page_results = r_strings[index].splitlines()
    i = 0
    print "formating results for: loadtime"
    print "# of values: %d" % len(page_results)
    for mypage in page_results[3:]:
      r = mypage.split(';')
      tmpf.write(result_format % (float(r[2]), testname + "_loadtime", tbox, i, date, test_configs[index][3], test_configs[index][4], "discrete", r[1]))
      i = i+1

  for index in range(len(tp_counters)):
    for count_type in config.COUNTERS:
      i = 0
      print "formating results for: " + count_type
      print "# of values: %d" % len(tp_counters[index][count_type])
      for value in tp_counters[index][count_type]:
        tmpf.write(result_format2 % (float(value), testname + "_" + count_type.replace("%", "Percent"), tbox, i, date, test_configs[index][3], test_configs[index][4], "discrete"))
        i = i+1


  print "finished formating results"
  tmpf.flush()
  tmpf.close()
  tmpf = open(filename, "r")
  file_data = tmpf.read()
  while True:
    try:
      ret = post_file.post_multipart(config.RESULTS_SERVER, config.RESULTS_LINK, [("key", "value")], [("filename", filename, file_data)
   ])
    except IOError:
      print "IOError"
    else:
      break
  print "completed sending results"
  links = process_Request(ret)
  tmpf.close()
  os.remove(filename)

  lines = links.split('\n')
  for line in lines:
    if line == "":
      continue
    values = line.split(":")
    linkName = values[1]
    if float(values[2]) > 0:
      linkName += "_T: " + values[2]
    else:
      linkName += "_1"
    url = url_format % (config.RESULTS_SERVER, values[0],)
    link = link_format % (url, linkName,)
    print "RETURN: " + link
  
if __name__=='__main__':

  # Read in each config file and run the tests on it.
  for i in range(1, len(sys.argv)):
    utils.debug("running test file " + sys.argv[i])
    test_file(sys.argv[i])

