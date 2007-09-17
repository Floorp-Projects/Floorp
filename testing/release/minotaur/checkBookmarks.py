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

import sgmllib
import re

Not_White_Space = re.compile('\S')

def debug(s):
  if (0):
    print s

# This class parses the bookmark file, making a listing of each
# anchor's title, address, and containing folder (if any). It does this by
# making use of the fact that the bookmarks.html file has this basic structure
# by default
# <DL>
#    <H3>Folder Title</H3>
#        <A href="link">Link Title</A>
# <DL>
# <A href="link">Link Title</A>
# and so on...
# We can depend on the SGML parser to walk sequentially through the file and
# call us back when it encounters a starting tag, ending tag, or enclosed data
class bookmarkParser(sgmllib.SGMLParser):

  def __init__(self, verbose=0):

    self.marker, self.IN_H3, self.IN_A = range(3)

    sgmllib.SGMLParser.__init__(self, verbose)
    self.isToolbar = False
    self.currentFolder = ""
    self.currentAnchor = ""
    self.bookmarkList = []

  def parseFile(self, fileName):
    bkmkFile = open(fileName, "r")
    htmlContent = bkmkFile.read()
    self.feed(htmlContent)
    self.close()

  # This is called when we hit an H3 tag
  def start_h3(self, attributes):
    self.marker = self.IN_H3
    self.isToolbar = False

    for attr in attributes:
      # Check that we are in the personal toolbar folder
      if (attr[0] == 'personal_toolbar_folder' and attr[1] == 'true'):
        self.isToolbar = True

 # Called when an anchor tag is hit
  def start_a(self, attributes):
    self.marker = self.IN_A
    for attr in attributes:
      if (attr[0] == "href"):
        debug("Found anchor link: " + attr[1])
        self.currentAnchor = attr[1]

  # Called when an anchor end tag is hit to reset the current anchor
  def end_a(self):
    debug("End A reset")
    self.currentAnchor = ""

  # Called when text data is encountered
  def handle_data(self, data):
    if (Not_White_Space.match(data)):
      debug("in non-whitespace data")
      # If we are inside an H3 link we are getting the folder name
      if self.marker == self.IN_H3:
        if self.isToolbar:
          debug("data:h3 is toolbar")
          self.currentFolder = "toolbar"
        else:
          debug("data:h3:not toolbar")
          self.currentFolder = data

      elif self.marker == self.IN_A:
        # Then we are inside an anchor tag - we now have the folder,
        # link and data
        debug("data:isA adding following: " + self.currentFolder + "," +
        self.currentAnchor + "," + data)
        self.bookmarkList.append( (self.currentFolder, self.currentAnchor,
                                   data) )

  # We have to include a "start" handler or the end handler won't be called
  # we really aren't interested in doing anything here
  def start_dl(self, attributes):
    return 1

  # Called when we hit an end DL tag to reset the folder selections
  def end_dl(self):
    debug("End DL reset")
    self.isToolbar = False
    self.currentFolder = ""
    self.currentAnchor = ""

  def getList(self):
    return self.bookmarkList

# This just does a linear search, but we have a sorted list because it's easier
# to create a sub list that way (compared to binary search)
# TODO: If it ever becomes a problem, change to binary search
def getFolderList(folderName, sortedList):
  fldrList = []
  fldrSet = False
  for s in sortedList:
    if s[0] == folderName:
      fldrList.append(s)
      fldrSet = True
    else:
      if fldrSet:
        # Then we know that we have completed reading all folders with
        # "folderName" so we can quit
          break
      else:
        # Then we have not yet begun to create our sublist, keep
        # walking sortedList
        continue
  return fldrList

def checkBookmarks(loc, bkmkFile, verifier, log):
  rtn = True
  parser = bookmarkParser()
  parser.parseFile(bkmkFile)

  # Verify the bookmarks now
  bkmkList = parser.getList()
  bkmkList.sort(lambda x,y: cmp(x[0], y[0]))

  verifiedBkmks = verifier.getElementList("bookmarks")

  # Now we compare the parsed list with the verified list
  for vElem in verifiedBkmks:
    fldrList = getFolderList(vElem.getAttribute("folder"), bkmkList)
    for f in fldrList:
      if (vElem.getAttribute("link") == f[1] and
          vElem.getAttribute("title") == f[2]):
        log.writeLog("\n" + vElem.getAttribute("title") + " bookmark PASSES")
      else:
        log.writeLog("\n" + vElem.getAttribute("title") + " bookmark FAILS")
        rtn = False
  return rtn
