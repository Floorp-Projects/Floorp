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

"""A list of constants containing the paths to programs and files
   needed by the performance testing scripts.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


OS = r'win32'

"""For some reason, can only get output from dump() in Firefox if
   it's run through cygwin bash.  So here's the path to cygwin.
"""
CYGWIN = r'c:\cygwin\bin\bash.exe -c'

"""The tinderbox scripts run sync between Ts runs, so we do, too."""
SYNC = r'c:\cygwin\bin\sync'

"""The path to the base profile directory to use for testing.  For the page
   load test to work, this profile should have its hostperm.1 file set to allow
   urls with scheme:file to open in new windows, and the preference to open
   new windows in a tab should be off.
"""
BASE_PROFILE_DIR = r'C:\win32\base_profile'

"""The directory the generated reports go into."""
REPORTS_DIR = r'c:\extension_perf_reports'

"""The path to the file url to load when initializing a new profile"""
INIT_URL = 'file:///c:/win32/initialize.html'

"""The path to the file url to load when collecting information from the browser"""
INFO_URL = 'file:///c:/win32/getInfo.html'

"""The path to the file url to load for startup test (Ts)"""
TS_URL = 'file:///c:/win32/startup_test/startup_test.html?begin='

"""Number of times to run startup test (Ts)"""
TS_NUM_RUNS = 5

"""The path to the file url to load for page load test (Tp)"""
TP_URL = 'http://localhost/page_load_test/framecycler.html'

"""Number of times the page load test (Tp) loads each page in the test."""
TP_NUM_CYCLES = 5

"""Resolution of counter sample data for page load test (Tp), in seconds
   (For example, if TP_RESOLUTION=1, sample counters every 1 second"""
TP_RESOLUTION = 1
  
"""Run page load test.
   For possible values of counters argument on Windows, see
   http://technet2.microsoft.com/WindowsServer/en/Library/86b5d116-6fb3-427b-af8c-9077162125fe1033.mspx?mfr=true
   Possible values on Linux:
     'Private Bytes', '% Processor Time', 'RSS'
"""

COUNTERS = ['Private Bytes', 'Working Set', '% Processor Time']

"""URL for the results server"""
RESULTS_SERVER = 'graphserver.url.here'
