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
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Clint Talbert.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Clint Talbert <ctalbert@mozilla.com>
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

from optparse import OptionParser
import platform

def getPlatform():
  print platform.system()

def getFxName(os):
  if os == "Darwin":
    print "firefox-bin"
  elif os == "Linux":
    print "firefox"
  elif os == "Windows":
    print "firefox.exe"

def main(os, fxname):
  # The options given determine the behavior
  # If no options -- return the OS
  # If OS AND fxname, return the firefox executable name on this OS
  # Anything else, fail.

  retval = ""

  if not os:
    getPlatform()
  elif os and fxname:
    getFxName(os)
  else:
    raise SystemExit("Invalid Command use getOsInfo --h for help")

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-o", "--os", dest="os",
                   help="OS identifer - either Darwin, Linux, or Windows can be\
                        obtained by calling without any params", metavar="OS")
  parser.add_option("-f", "--firefoxName", action="store_true", dest="fxname", default=False,
                    help="Firefox executable name on this platform requires OS")
  (options, args) = parser.parse_args()

  # Call Main
  main(options.os, options.fxname)
