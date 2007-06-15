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
#   Ben Hearsum    <bhearsum@wittydomain.com> (OS independence)
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

"""A set of functions to run the Tp test.

   The Tp test measures page load times in Firefox.  It does this with a
   JavaScript script that opens a new window and cycles through page loads
   from the local disk, timing each one.  The script dumps the sum of the
   mean times to open each page, and the standard deviation, to standard out.
   We can also measure performance attributes during the test.  See
   http://technet2.microsoft.com/WindowsServer/en/Library/86b5d116-6fb3-427b-af8c-9077162125fe1033.mspx?mfr=true
   for a description of what can be measured.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import os
import re
import shutil
import time
import sys

import ffprocess
import ffprofile
import ffinfo
import config

if config.OS == "linux":
    from tp_linux import *
elif config.OS == "win32":
    from tp_win32 import *


# Regular expression to get stats for page load test (Tp)
TP_REGEX = re.compile('__start_page_load_report(.*)__end_page_load_report',
                      re.DOTALL | re.MULTILINE)
TP_REGEX_FAIL = re.compile('__FAIL(.*)__FAIL', re.DOTALL|re.MULTILINE)


def RunPltTests(profile_configs,
                num_cycles,
                counters,
                resolution):
  """Runs the Page Load tests with profiles created from the given
     base diectory and list of configuations.
  
  Args:
    profile_configs:  Array of configuration options for each profile.
      These are of the format:
      [{prefname:prevalue,prefname2:prefvalue2},
       {extensionguid:path_to_extension}],[{prefname...
    num_cycles: Number of times to cycle through all the urls on each test.
    counters: Array of counters ("% Processor Time", "Working Set", etc)
              See http://technet2.microsoft.com/WindowsServer/en/Library/86b5d116-6fb3-427b-af8c-9077162125fe1033.mspx?mfr=true
              for a list of available counters and their descriptions.
    resolution: Time (in seconds) between collecting counters
  
  Returns:
    A tuple containing:
      An array of plt results for each run.  For example:
        ["mean: 150.30\nstdd:34.2", "mean 170.33\nstdd:22.4"]
      An array of counter data from each run.  For example:
        [{"counter1": [1, 2, 3], "counter2":[4,5,6]},
         {"counter1":[1,3,5], "counter2":[2,4,6]}]
  """
 
  res = 0
  counter_data = []
  plt_results = []
  results_string = []
  for pconfig in profile_configs:
    print "in tp"
    print pconfig
    sys.stdout.flush()
    rstring = ""
    # Create the new profile
    profile_dir = ffprofile.CreateTempProfileDir(pconfig[5],
                                                 pconfig[0],
                                                 pconfig[1])
    print "created profile" 
    # Run Firefox once with new profile so initializing it doesn't cause
    # a performance hit, and the second Firefox that gets created is properly
    # terminated.
    ffprofile.InitializeNewProfile(pconfig[2], profile_dir)
    ffinfo.GetMetricsFromBrowser(pconfig[2], profile_dir)
    print "initialized firefox"
    sys.stdout.flush()
    ffprocess.SyncAndSleep()

    # Run the plt test for this profile
    timeout = 10000
    total_time = 0
    output = ''
    url = config.TP_URL + '?cycles=' + str(num_cycles)
    command_line = ffprocess.GenerateFirefoxCommandLine(pconfig[2], profile_dir, url)
    handle = os.popen(command_line)
    # give firefox a chance to open
    time.sleep(1)

    cm = CounterManager("firefox", counters)

    cm.startMonitor()

    counts = {}
    for counter in counters:
      counts[counter] = []
    
    while total_time < timeout:
    
      # Sleep for [resolution] seconds
      time.sleep(resolution)
      total_time += resolution
      
      # Get the output from all the possible counters
      for count_type in counters:
        val = cm.getCounterValue(count_type)

        if (val):
          counts[count_type].append(val)

      # Check to see if page load times were outputted
      (bytes, current_output) = ffprocess.NonBlockingReadProcessOutput(handle)
      output += current_output
      match = TP_REGEX.search(output)
      if match:
        rstring += match.group(1)
        plt_results.append(match.group(1))
	res = 1
        break
      match = TP_REGEX_FAIL.search(output)
      if match:
        rstring += match.group(1)
        plt_results.append(match.group(1))
        break

    cm.stopMonitor()
   
    print "got tp results from browser" 

    # Firefox should have exited cleanly, but close it if it doesn't
    # after 2 seconds.
    time.sleep(2)
    ffprocess.TerminateAllProcesses("firefox")
    ffprocess.SyncAndSleep()
    
    # Delete the temp profile directory  Make it writeable first,
    # because every once in a while Firefox seems to drop a read-only
    # file into it.
    ffprofile.MakeDirectoryContentsWritable(profile_dir)
    shutil.rmtree(profile_dir)
    
    counter_data.append(counts)
    results_string.append(rstring)

  return (res, results_string, plt_results, counter_data)
