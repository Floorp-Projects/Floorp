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
import difflib
from checkBookmarks import bookmarkParser
from logAppender import LogAppender, stderrCatcher
import sys

DIFFBKMK_DBG = False

def debug(s):
  if DIFFBKMK_DBG:
    print s

# The Main function
def main(left, right, log):
  # Instantiate the log writer
  lw = LogAppender(log)
  # Redirect stderr
  sys.stderr = stderrCatcher(lw)

  # Parse the left hand file
  leftParser = bookmarkParser(isDebug=DIFFBKMK_DBG)
  leftParser.parseFile(left)

  # Parse the right hand file
  rightParser = bookmarkParser(isDebug=DIFFBKMK_DBG)
  rightParser.parseFile(right)

  # Now we compare the lists generated from the parsing and they should be
  # identical, and they are sorted by <folder> and then by <link title>
  leftList = leftParser.getList()
  rightList = rightParser.getList()

  # Handle the case where we are missing a file
  if len(leftList) == 0:
    lw.writeLog("**** BOOKMARKS REFERENCE FILE IS MISSING ****")
    raise SystemExit("**** BOOKMARKS REFERENCE FILE IS MISSING ****")
  elif len(rightList) == 0:
    lw.writeLog("**** BOOKMARKS DATA FOR BUILD UNDER TEST IS MISSING ****")
    raise SystemExit("**** BOOKMARKS DATA FOR BUILD UNDER TEST IS MISSING ****")

  leftlines = []
  for lentry in leftList:
    leftlines.append(lentry[0] + lentry[1] + lentry[2] + lentry[3] + lentry[4] + "\n")

  rightlines = []
  for rentry in rightList:
    rightlines.append(rentry[0] + rentry[1] + rentry[2] + rentry[3] + rentry[4] + "\n")
  
  # Create the diff.  Here's how it works.  These are flattened sorted lists of
  # bookmarks.  For ease of viewing, in their diffs, they look like this:
  # folder=<bookmark folder> title=<link title> url=<link href> feedurl=<feedurl (if present)> icon=<icon>
  # The diff takes the two sets of lines and does a unified diff on them.
  diff = difflib.unified_diff(leftlines, rightlines)
  lw.writelines(diff)

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-l", "--leftFile", dest="left",
                   help="Bookmarks HTML file 1", metavar="LEFT_FILE")
  parser.add_option("-r", "--rightFile", dest="right",
                    help="Bookmarks HTML file 2", metavar="RIGHT_FILE")
  parser.add_option("-f", "--LogFile", dest="log",
                    help="The file where the log output should go",
                    metavar="LOGFILE")
  parser.add_option("-d", "--Debug", dest="isDebug", default=False,
                    help="Turn on debug output by specifying -d true")

  (options, args) = parser.parse_args()

  if options.isDebug:
    DIFFBKMK_DBG = True

  # Call Main
  main(options.left, options.right, options.log)
