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
import syck
import sys

import report
import paths
import tp
import ts


# Number of times to run startup test (Ts)
TS_NUM_RUNS = 5

# Number of times the page load test (Tp) loads each page in the test.
TP_NUM_CYCLES = 7

# Resolution of counter sample data for page load test (Tp), in seconds
# (For example, if TP_RESOLUTION=1, sample counters every 1 second
TP_RESOLUTION = 1


def test_file(filename):
  """Runs the Ts and Tp tests on the given config file and generates a report.
  
  Args:
    filename: the name of the file to run the tests on
  """
  
  test_configs = []
  test_names = []
  title = ''
  filename_prefix = ''
  
  # Read in the profile info from the YAML config file
  config_file = open(filename, 'r')
  yaml = syck.load(config_file)
  config_file.close()
  for item in yaml:
    if item == 'title':
      title = yaml[item]
    elif item == 'filename':
      filename_prefix = yaml[item]
    else:
      new_config = [yaml[item]['preferences'],
                    yaml[item]['extensions'],
                    yaml[item]['firefox']]
      test_configs.append(new_config)
      test_names.append(item)
  config_file.close()
  
  # Run startup time test
  ts_times = ts.RunStartupTests(paths.BASE_PROFILE_DIR,
                                test_configs,
                                TS_NUM_RUNS)
  
  # Run page load test.  For possible values of counters argument, see
  # http://technet2.microsoft.com/WindowsServer/en/Library/86b5d116-6fb3-427b-af8c-9077162125fe1033.mspx?mfr=true
  (tp_times, tp_counters) = tp.RunPltTests(paths.BASE_PROFILE_DIR,
                                           test_configs,
                                           TP_NUM_CYCLES,
                                           ['Private Bytes', 'Working Set', '% Processor Time'],
                                           TP_RESOLUTION)
  
  # Generate a report of the results.
  report.GenerateReport(title,
                        filename_prefix,
                        test_names,
                        ts_times,
                        tp_times,
                        tp_counters,
                        TP_RESOLUTION)


if __name__=='__main__':

  # Read in each config file and run the tests on it.
  for i in range(1, len(sys.argv)):
    test_file(sys.argv[i])

