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

"""A generic means of running an URL based browser test
   follows the following steps
     - creates a profile
     - tests the profile
     - gets metrics for the current test environment
     - loads the url
     - collects info on any counters while test runs
     - waits for a 'dump' from the browser
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import platform
import os
import re
import shutil
import time
import sys
import subprocess
import utils

import ffprocess
import ffsetup

if platform.system() == "Linux":
    from cmanager_linux import *
    platform_type = 'unix_'
elif platform.system() in ("Windows", "Microsoft"):
    from cmanager_win32 import *
    platform_type = 'win_'
elif platform.system() == "Darwin":
    from cmanager_mac import *
    platform_type = 'unix_'


# Regula expression for getting results from most tests
RESULTS_REGEX = re.compile('__start_report(.*)__end_report',
                      re.DOTALL | re.MULTILINE)
# Regular expression to get stats for page load test (Tp) - should go away once data passing is standardized
RESULTS_TP_REGEX = re.compile('__start_tp_report(.*)__end_tp_report',
                      re.DOTALL | re.MULTILINE)
RESULTS_GENERIC = re.compile('(.*)', re.DOTALL | re.MULTILINE)
RESULTS_REGEX_FAIL = re.compile('__FAIL(.*)__FAIL', re.DOTALL|re.MULTILINE)


def runTest(browser_config, test_config):
  """
  Runs an url based test on the browser as specified in the browser_config dictionary
  
  Args:
    browser_config:  Dictionary of configuration options for the browser (paths, prefs, etc)
    test_config   :  Dictionary of configuration for the given test (url, cycles, counters, etc)
  
  """
 
  res = 0
  utils.debug("operating with platform_type : " + platform_type)
  counters = test_config[platform_type + 'counters']
  resolution = test_config['resolution']
  all_browser_results = []
  all_counter_results = []
  utils.setEnvironmentVars(browser_config['env'])

  # add any provided directories to the installed firefox
  for dir in browser_config['dirs']:
    ffsetup.InstallInBrowser(browser_config['firefox'], browser_config['dirs'][dir])
  
  if browser_config["profile_path"] != {}:
      # Create the new profile
      temp_dir, profile_dir = ffsetup.CreateTempProfileDir(browser_config['profile_path'],
                                                 browser_config['preferences'],
                                                 browser_config['extensions'])
      utils.debug("created profile") 
      # Run Firefox once with new profile so initializing it doesn't cause
      # a performance hit, and the second Firefox that gets created is properly
      # terminated.
      
      res = ffsetup.InitializeNewProfile(browser_config['firefox'], profile_dir, browser_config['init_url'])
      if not res:
          print "FAIL: couldn't initialize firefox"
          return (res, all_browser_results, all_counter_results)
      res = 0
  else:
      # no profile path was set in the config, set the profile_dir to an empty string.
      profile_dir = ""

  utils.debug("initialized firefox")
  sys.stdout.flush()
  ffprocess.Sleep()

  for i in range(test_config['cycles']):
    # Run the test 
    browser_results = ""
    timeout = 28800 # 8 hours
    total_time = 0
    output = ''
    url = test_config['url']
    if 'url_mod' in test_config:
      url += eval(test_config['url_mod']) 
    command_line = ffprocess.GenerateFirefoxCommandLine(browser_config['firefox'], profile_dir, url)

    utils.debug("command line: " + command_line)

    process = subprocess.Popen(command_line, stdout=subprocess.PIPE, universal_newlines=True, shell=True, bufsize=0, env=os.environ)
    handle = process.stdout

    #give firefox a chance to open
    # this could mean that we are losing the first couple of data points as the tests starts, but if we don't provide
    # some time for the browser to start we have trouble connecting the CounterManager to it
    ffprocess.Sleep()
    #set up the counters for this test
    cm = CounterManager("firefox", counters)
    cm.startMonitor()
    counter_results = {}
    for counter in counters:
      counter_results[counter] = []
   
    busted = False 
    while total_time < timeout:
      # Sleep for [resolution] seconds
      time.sleep(resolution)
      total_time += resolution
      
      # Get the output from all the possible counters
      for count_type in counters:
        val = cm.getCounterValue(count_type)

        if (val):
          counter_results[count_type].append(val)

      # Check to see if page load times were outputted
      (bytes, current_output) = ffprocess.NonBlockingReadProcessOutput(handle)
      output += current_output
      match = RESULTS_GENERIC.search(current_output)
      if match:
        if match.group(1):
          utils.noisy(match.group(1))
      match = RESULTS_REGEX.search(output)
      if match:
        browser_results += match.group(1)
        utils.debug("Matched basic results: " + browser_results)
        res = 1
        break
      #TODO: this a stop gap until all of the tests start outputting the same format
      match = RESULTS_TP_REGEX.search(output)
      if match:
        browser_results += match.group(1)
        utils.debug("Matched tp results: " + browser_results)
        res = 1
        break
      match = RESULTS_REGEX_FAIL.search(output)
      if match:
        browser_results += match.group(1)
        utils.debug("Matched fail results: " + browser_results)
        print "FAIL: " + match.group(1)
        break

      #ensure that the browser is still running
      #check at intervals of 60 - this is just to cut down on load 
      #use the busted check to ensure that we aren't catching a bad time slice where the browser has
      # completed the test and closed but we haven't picked up the result yet
      if busted:
        ffprocess.TerminateAllProcesses("firefox")
        ffprocess.TerminateAllProcesses("crashreporter")
        print "FAIL: browser crash"
        break
      if (total_time % 60 == 0): 
        if ffprocess.ProcessesWithNameExist("crashreporter") or not ffprocess.ProcessesWithNameExist("firefox"):
          busted = True

    if total_time >= timeout:
      print "FAIL: timeout from test"

    #stop the counter manager since this test is complete
    cm.stopMonitor()

    utils.debug("Completed test with: " + browser_results)
    #kill any remaining firefox processes
    ffprocess.Sleep()
    ffprocess.TerminateAllProcesses("firefox")
    all_browser_results.append(browser_results)
    all_counter_results.append(counter_results)
    
  # Delete the temp profile directory  Make it writeable first,
  # because every once in a while Firefox seems to drop a read-only
  # file into it.
  ffsetup.MakeDirectoryContentsWritable(temp_dir)
  shutil.rmtree(temp_dir)
    
  utils.restoreEnvironmentVars()
    
  return (res, all_browser_results, all_counter_results)
