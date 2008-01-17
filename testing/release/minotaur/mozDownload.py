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

import urllib
import urlparse
import shutil
import subprocess
from optparse import OptionParser
import os

_mozdownload_debug = False

def debug(str):
  if _mozdownload_debug:
    print "DEBUG DOWNLOAD.py: " + str

# Overrides some of the FancyURLopener functionality so that we can send in
# our auth info.  It only tries once. If the auth info is wrong, it throws.
class mozURLopener(urllib.FancyURLopener):
  def __init__(self, **kwargs):
    if kwargs['user'] != "":
      self.user = kwargs['user']
      self.passwd = kwargs['passwd']
    else:
      self.user = ""
      self.passwd = ""
    self.triedPassword = False

    urllib.FancyURLopener.__init__(self)

  def prompt_user_passwd (self, host, realm):
    debug("mozURLopener: Sending Password")
    self.triedPassword = True
    return(self.user, self.passwd)

  def http_error_401(self, url, fp, errcode, errmsg, headers, data=None):
    debug("mozURLOpener: GOT a 401!!!")
    if not self.triedPassword:
      return urllib.FancyURLopener.http_error_401(self, url, fp, errcode, errmsg, headers, data=None)
    else:
      # Password was incorrect, throw a 401
      raise IOError, 401
      return None

class MozDownloader:
  def __init__(self, **kwargs):
    assert (kwargs['url'] != "" and kwargs['url'] != None)
    assert (kwargs['dest'] != "" and kwargs['dest'] != None)
    self.url = kwargs['url']
    self.dest = kwargs['dest']
    self.error = 0
    self.user = kwargs['user']
    self.passwd = kwargs['password']

  def download(self):
    # Attempt to use urllib for this instead of wget
    try:
      opener = mozURLopener(user=self.user, passwd=self.passwd)
      data = opener.open(self.url)
      #Ensure directory exists
      self.ensureDest()
      destfile = open(self.dest, "wb")
      destfile.write(data.read())
      destfile.close()
    except:
      print "Download did not work - is your username and password correct?"

  def ensureDest(self):
    try:
      # Work around the fact that the os.path module doesn't understand ~/ paths
      if self.dest[0] == "~":
        self.dest = self.dest.replace("~", "${HOME}", 1)
      headpath = os.path.split(self.dest)[0]
      headpath = os.path.expandvars(headpath)
      try:
        if not os.path.exists(headpath):
          os.makedirs(headpath)
      except:
        print "Error creating directory for download"
    except:
      self.error = 1

  def moveToDest(self):
    try:
      # Grab the name of the downloaded file from the URL
      parsedUrl = urlparse.urlparse(self.url)
      path = parsedUrl[2]
      pathElements = path.split("/")
      filename = pathElements[len(pathElements) - 1]
      print filename

      # Create directory - we assume that the name of the file is the last
      # parameter on the path
      if self.dest[0] == "~":
        self.dest = self.dest.replace("~", "${HOME}", 1)
      headpath = os.path.split(self.dest)[0]
      headpath = os.path.expandvars(headpath)
      try:
        if not os.path.exists(headpath):
          os.makedirs(headpath)
      except:
        print "Error creating destination directory"

      # Move it to destination location and file name
      self.dest = os.path.expandvars(self.dest)
      debug(self.dest)
      shutil.move("./" + urllib.unquote(filename), self.dest)
    except:
      self.error = 1

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-d", "--Destination", dest="dest",
                   help="Destination file to download to",
                   metavar="DEST_FILE")
  parser.add_option("-u", "--URL", dest="url",
                    help="URL to download from", metavar="URL")
  parser.add_option("-n", "--userName", dest="user",
                    help="User name if needed (optional)",
                    metavar="USERNAME")
  parser.add_option("-p", "--Password", dest="password",
                    help="Password for User name (optional)",
                    metavar="PASSWORD")

  (options, args) = parser.parse_args()

  # Run it
  mozDwnld = MozDownloader(url=options.url, dest=options.dest, user=options.user,
                           password=options.password)
  mozDwnld.download()
