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
from checkBookmarks import bookmarkParser
from logAppender import LogAppender

# The Main function
def main(left, right, log):
  # Instantiate the log writer
  lw = LogAppender(log)

  # Parse the left hand file
  leftParser = bookmarkParser()
  leftParser.parseFile(left)

  # Parse the right hand file
  rightParser = bookmarkParser()
  rightParser.parseFile(right)

  # Now we compare the lists generated from the parsing and they should be
  # identical
  leftList = leftParser.getList()
  rightList = rightParser.getList()

  if len(leftList) <> len(rightList):
    lw.writeLog("Bookmarks lists are not the same length!")
    raise SystemExit("Bookmark lists not same length, test fails")

  for lentry, rentry in zip(leftList, rightList):
    if lentry <> rentry:
      lw.writeLog("Error found entries that do not match")
      lw.writeLog("Left side: " + lentry[0] + lentry[1])
      lw.writeLog("Right side: " + rentry[0] + rentry[1])
      raise SystemExit("Bookmark entries do not match, test fails")

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-l", "--leftFile", dest="left",
                   help="Bookmarks HTML file 1", metavar="LEFT_FILE")
  parser.add_option("-r", "--rightFile", dest="right",
                    help="Bookmarks HTML file 2", metavar="RIGHT_FILE")
  parser.add_option("-f", "--LogFile", dest="log",
                    help="The file where the log output should go",
                    metavar="LOGFILE")
  (options, args) = parser.parse_args()

  # Call Main
  main(options.left, options.right, options.log)
