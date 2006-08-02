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

"""A set of functions to run the Ts test.

   The Ts test measures startup time for Firefox.  It does this by running
   Firefox with a special page that takes an argument containing the current
   time, and when the page loads (and Firefox is fully started), it writes
   the difference between that time and the now-current time to stdout.  The
   test is run a few times for different profiles, so we can tell if our
   extension affects startup time.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import re
import shutil
import time

import ffprocess
import ffprofile
import paths


# Regular expression to get the time for startup test (Ts)
TS_REGEX = re.compile('__startuptime,(\d*)')


def IsInteger(number):
  """Helper function to determine if a variable is an integer"""
  try:
    int(number)
  except:
    return False

  return True


def RunStartupTest(firefox_path, profile_dir, num_runs, timeout):
  """Runs the Firefox startup test (Ts) for the given number
     of times and returns the output.  If running with a brand
     new profile, make sure to call InitializeNewProfile() first.

  Args:
    firefox_path: The path to the firefox exe to run
    profile_dir: The directory of the profile to use
    num_runs: The number of times to run the startup test
              (1 extra dummy run at the beginning will be thrown out)
    timeout:  The time in seconds to wait before failing and terminating Firefox

  Returns:
    Array containing startup times in milliseconds
  """

  startup_times = []
  for i in range(-1, num_runs):
    # Make sure we don't get "Firefox is already running..." errors
    ffprocess.SyncAndSleep()

    # Create a command line that passes in the url of the startup test
    # with the current time as the begin_time argument
    time_arg = int(time.time() * 1000)
    url = paths.TS_URL + str(time_arg)
    command_line = ffprocess.GenerateFirefoxCommandLine(firefox_path, profile_dir, url)
    (match, timed_out) = ffprocess.RunProcessAndWaitForOutput(command_line,
                                                              'firefox',
                                                              TS_REGEX,
                                                              timeout)
    if timed_out or not IsInteger(match):
      match = None
    if i > -1 and match and match > 0:
      startup_times.append(match)

  return startup_times


def RunStartupTests(source_profile_dir, profile_configs, num_runs):
  """Runs the startup tests with profiles created from the
     given base profile directory and list of configurations.

  Args:
    source_profile_dir:  Full path to base directory to copy profile from.
    profile_configs:  Array of configuration options for each profile.
      These are of the format:
      [{prefname:prevalue,prefname2:prefvalue2},{extensionguid:path_to_extension}],[{prefname...
    num_runs: Number of times to run startup tests for each profile

  Returns:
    Array of arrays of startup times, one for each profile.
  """

  all_times = []
  for config in profile_configs:
    # Create the new profile
    profile_dir = ffprofile.CreateTempProfileDir(source_profile_dir,
                                                 config[0],
                                                 config[1])

    # Run Firefox once with new profile so initializing it doesn't
    # cause a performance hit, and the second Firefox that gets
    # created is properly terminated.
    ffprofile.InitializeNewProfile(config[2], profile_dir)

    # Run the startup tests for this profile and log the results.
    times = RunStartupTest(config[2], profile_dir, 5, 30)
    all_times.append(times)

    # Delete the temp profile directory.  Make it writeable first,
    # because every once in a while Firefox seems to drop a read-only
    # file into it.
    ffprocess.SyncAndSleep()
    ffprofile.MakeDirectoryContentsWritable(profile_dir)
    shutil.rmtree(profile_dir)

  return all_times
