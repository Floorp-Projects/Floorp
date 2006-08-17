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

"""A set of functions to set up a Firefox profile with the correct
   preferences and extensions in the given directory.

   Sets up the profile by copying from a base directory, editing the
   prefs.js file to set the prefs, and creating a file to link to each
   extension.  The profile is run with Firefox to make sure it is fully
   initialized and won't cause extra startup time on the first run.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import os
import re
import shutil
import tempfile
import time

import ffprocess
import paths


def PrefString(name, value, newline):
  """Helper function to create a pref string for Firefox profile prefs.js
     in the form 'user_pref("name", value);<newline>'

  Args:
    name: String containing name of pref
    value: String containing value of pref
    newline: Line ending to use, i.e. '\n' or '\r\n'

  Returns:
    String containing 'user_pref("name", value);<newline>'
  """

  out_value = str(value)
  if type(value) == bool:
    # Write bools as "true"/"false", not "True"/"False".
    out_value = out_value.lower()
  if type(value) == str:
    # Write strings with quotes around them.
    out_value = '"%s"' % value
  return 'user_pref("%s", %s);%s' % (name, out_value, newline)


def MakeDirectoryContentsWritable(dirname):
  """Recursively makes all the contents of a directory writable.
     Uses os.chmod(filename, 0777), which works on Windows.

  Args:
    dirname: Name of the directory to make contents writable.
  """

  for (root, dirs, files) in os.walk(dirname):
    os.chmod(root, 0777)
    for filename in files:
      os.chmod(os.path.join(root, filename), 0777)


def CreateTempProfileDir(source_profile, prefs, extensions):
  """Creates a temporary profile directory from the source profile directory
     and adds the given prefs and links to extensions.

  Args:
    source_profile: String containing the absolute path of the source profile
                    directory to copy from.
    prefs: Preferences to set in the prefs.js file of the new profile.  Format:
           {"PrefName1" : "PrefValue1", "PrefName2" : "PrefValue2"}
    extensions: Guids and paths of extensions to link to.  Format:
                {"{GUID1}" : "c:\\Path\\to\\ext1", "{GUID2}", "c:\\Path\\to\\ext2"}

  Returns:
    String containing the absolute path of the profile directory.
  """

  # Create a temporary directory for the profile, and copy the
  # source profile to it.
  profile_dir = tempfile.mkdtemp()
  profile_dir = os.path.join(profile_dir, 'profile')
  shutil.copytree(source_profile, profile_dir)
  MakeDirectoryContentsWritable(profile_dir)

  # Copy the user-set prefs to user.js
  user_js_filename = os.path.join(profile_dir, 'user.js')
  user_js_file = open(user_js_filename, 'w')
  for pref in prefs:
    user_js_file.write(PrefString(pref, prefs[pref], '\n'))
  user_js_file.close()

  # Add links to all the extensions.
  extension_dir = os.path.join(profile_dir, "extensions")
  if not os.path.exists(extension_dir):
    os.makedirs(extension_dir)
  for extension in extensions:
    link_file = open(os.path.join(extension_dir, extension), 'w')
    link_file.write(extensions[extension])
    link_file.close()

  return profile_dir


def InitializeNewProfile(firefox_path, profile_dir):
  """Runs Firefox with the new profile directory, to negate any performance
     hit that could occur as a result of starting up with a new profile.  
     Also kills the "extra" Firefox that gets spawned the first time Firefox
     is run with a new profile.

  Args:
    firefox_path: String containing the path to the Firefox exe
    profile_dir: The full path to the profile directory to load
  """

  # Run Firefox with the new profile directory
  cmd = ffprocess.GenerateFirefoxCommandLine(firefox_path, profile_dir, paths.INIT_URL)
  handle = os.popen(cmd)

  # Wait for Firefox to shut down and restart with the new profile,
  # then kill the new instance if it doesn't close itself.
  time_elapsed = 0
  while time_elapsed < 30:
    time_elapsed += 5
    time.sleep(5)
    if not ffprocess.ProcessesWithNameExist("firefox"):
      return

  ffprocess.TerminateAllProcesses("firefox")
