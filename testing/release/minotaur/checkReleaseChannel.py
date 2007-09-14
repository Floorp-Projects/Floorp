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

import re
from optparse import OptionParser
from logAppender import LogAppender

aus2link = re.compile(".*https:\/\/aus2.mozilla.org.*")

def checkHttpLog(httpLogFile, releaseChannel):
  result = False
  try:
    httpFile = open(httpLogFile, "r")
  except IOError:
    return result, "Http Log File Not Found"
  for line in httpFile:
    if aus2link.match(line):
      # This line should contain our release channel
      if line.find(releaseChannel) > 0:
        result = True, ""
        break
  return result, "Unable to find release chanel in HTTP Debug Log"

def main(httpFile, releaseFile, log):
  lf = LogAppender(log)
  rf = open(releaseFile, "r")
  # Ensure we don't pick up spurious newlines
  channel = rf.readline().split("\n")
  result, reason = checkHttpLog(httpFile, channel[0])

  if not result:
    lf.writeLog(reason)
    raise SystemExit("Release Update Channel not found. Test Fails")

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-d", "--DebugHttpLog", dest="httpFile",
                   help="Debug Http Log File", metavar="HTTP_LOG_FILE")
  parser.add_option("-r", "--ReleaseChannelFile", dest="releaseFile",
                    help="Text File with release channel name on first line",
                    metavar="RELEASE_FILE")
  parser.add_option("-l", "--LogFile", dest="log",
                    help="The file where the log output should go",
                    metavar="LOGFILE")
  (options, args) = parser.parse_args()

  # Call Main
  main(options.httpFile, options.releaseFile, options.log)
