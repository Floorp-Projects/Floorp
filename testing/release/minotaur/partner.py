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

from optparse import OptionParser
import platform
import subprocess
import re
import sys

from mozDownload import MozDownloader
from mozInstall import MozInstaller, MozUninstaller

isDebug = True

def debug(s):
  if isDebug:
    print "DEBUG: ",
    print s

generalerr = re.compile(".*error.*")

def getPlatformDirName():
  os = platform.system()
  if (os == "Darwin"):
    return "mac"
  elif (os == "Linux"):
    return "linux-i686"
  else:
    return "win32"

def getPlatformFirefoxName(version):
  os = platform.system()
  if (os == "Darwin"):
    return "Firefox%20" + version + ".dmg"
  elif (os == "Linux"):
    return "firefox-" + version + ".tar.gz"
  else:
    return "Firefox%20Setup%20" + version + ".exe"

def getDownloadLocation(partner, loc):
  os = platform.system()
  if (os == "Darwin"):
    return "~/minotaur-download/" + partner + "/" + loc + "/firefoxInst.dmg"
  elif  (os == "Linux"):
    return "~/minotaur-download/" + partner + "/" + loc + "/firefoxInst.tar.gz"
  else:
    return "c:/minotaur-download/" + partner + "/" + loc + "/firefoxInst.exe"

def getInstallLocation(partner, loc, isInst):
  os = platform.system()
  if (os == "Darwin"):
    if isInst:
      return "~/minotaur-build/" + partner + "/" + loc
    else:
      return "~/minotaur-build/" + partner + "/" + loc + "/Firefox.app/Contents/MacOS"
  elif  (os == "Linux"):
    if isInst:
      return "~/minotaur-build/" + partner + "/" + loc
    else:
      return "~/minotaur-build/" + partner + "/" + loc + "/firefox"
  else:
    return "c:/minotaur-build/" + partner + "/" + loc

def doDownload(partner, loc, url, minDir, creds):
  result = True
  dwnlddir = getDownloadLocation(partner, loc)
  user = None
  passwd = None
  if creds:
    user, passwd = creds.split(":")

  mozDwnld = MozDownloader(url=url, dest=dwnlddir, user=user,
                           password=passwd)
  mozDwnld.download()

  print "Downloading locale: " + loc
  if result:
    print "Result: DOWNLOADED"
  else:
    print "Result: NOT FOUND"
  print "==========================="
  return result

def doInstall(branch, partner, loc):
  # Installs the build using the installbuild.sh script
  #installs into <os>/minotaur-builds/partner/locale
  print " DEBUG: doInstall"
  result = True
  installdir = getInstallLocation(partner, loc, True)
  dwnlddir = getDownloadLocation(partner, loc)

  try:
    installer = MozInstaller(src = dwnlddir, dest = installdir, branch = branch,
                             productName = "firefox")
  except:
    print "DEBUG: mozinstaller threw"
    result = False

  print "========================="
  print "Installing locale: " + loc
  if result:
    print "Result: Installed"
  else:
    print "Result: ERROR!!"
  return result

def checkEULA(partner, loc, minDir, extName):
  # Checks for the existance of the EULA pref and writes out its pre-install
  # value.  Minotuar will pick this up and report the initial value in its diff.
  if (extName):
    installDir = getInstallLocation(partner, loc, False)
    args = "sh " + minDir + "/grabEULA.sh -f " + installDir + " -m " + minDir + " -p " + extName
    print "EULA args: " + args
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, shell=True)
    proc.wait()
    print proc.stdout

def doMinotaur(partner, loc, minDir, vFiles, ausparam, version):
  # If vFiles exist, then run full comparison run comparing against those files
  # otherwise, create those files put the created files into partner/locale
  # directories within the minotaur area
  result = True
  installDir = getInstallLocation(partner, loc, False)
  args = "sh minotaur.sh -n " + partner + " -m " + minDir + " -f " + installDir
  args += " -l " + loc + " -v " + version
  debug("Minotaur args: " + args)
  if vFiles:
    # Then we do a full run, with comparisons
    writeReleaseChannel(vFiles, loc, ausparam)
    outputFile = vFiles + "/" + loc + "/" + "test-output.xml"
    bkmkFile = vFiles + "/" + loc + "/" + "test-bookmarks.html"
    releaseFile = vFiles + "/" + loc + "/" + "release-channel.txt"
    args += " -o " + outputFile + " -b " + bkmkFile + " -c " + releaseFile
    proc = subprocess.Popen(args, shell=True)
    proc.wait()
  else:
    # Then we generate files for future reference
    proc = subprocess.Popen(args, shell=True)
    proc.wait()
  return result

def doUninstall(branch, partner, loc):
  # Uninstalls the build installed to <os>/minotuar-builds/partner/locale
  result = True
  installDir = getInstallLocation(partner, loc, True)
  debug("Calling uninstall")
  uninstaller = MozUninstaller(dest = installDir, branch = branch,
                               productName = "firefox")
  return result

def writeReleaseChannel(vFiles, loc, aus):
  filepath = vFiles + "/" +  loc + "/release-channel.txt"
  file = open(filepath, "w")
  file.write(aus)

def main(branch, version, url, partner, vFiles, minDir, extName, creds, aus,
         l10nFile):
  # First we craft the URL, assuming that the next directory down is the OS so
  # we append that.
  plat = getPlatformDirName()
  url += "/" + getPlatformDirName()

  fxname = getPlatformFirefoxName(version)

  try:
    l10nList = open(l10nFile, "r")
  except IOError:
    print "Unable to find L10N File!"

  for loc in l10nList:
    loc = loc.strip()
    # Attempt to download every locale.
    debug("BEGINNING DOWNLOAD")
    found = doDownload(partner, loc, url + "/" + loc + "/" + fxname, minDir, creds)
    found = True
    if found:
      debug("BEGINNING INSTALL")
      isInstalled = doInstall(branch, partner, loc)
      if isInstalled:
        debug("CHECKING EULA")
        checkEULA(partner, loc, minDir, extName)
        debug("RUNNING MINOTAUR")
        doMinotaur(partner, loc, minDir, vFiles, aus, version)
        debug("RUNNING UNINSTALL")
        doUninstall(branch, partner, loc)

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-p", "--Partner", dest="partner", help="Partner Name",
                    metavar="PARTNER_NAME")
  parser.add_option("-b", "--Branch", dest="branch",
                   help="Gecko Branch: 1.8.0|1.8.1|1.9", metavar="BRANCH")
  parser.add_option("-v", "--Version ", dest="version",
                    help="version of firefox to be tested",
                    metavar="FIREFOX_VERSION")
  parser.add_option("-u", "--UrlToBuild", dest="url",
                    help="URL to top level build location, above the OS directories",
                    metavar="URL")
  parser.add_option("-f", "--VerificationFileLocation", dest="verificationFiles",
                    help="location of verification files, leave blank to create verification files",
                    metavar="VER_FILES")
  parser.add_option("-m", "--MinotaurDirectory", dest="minDir",
                    help="Directory of the Minotuar code",
                    metavar="MINOTAUR_DIR")
  parser.add_option("-e", "--ExtensionName", dest="extName",
                    help="Name of the partner extension.  Only needed if Partner has EULA",
                    metavar="EXT_NAME")
  parser.add_option("-c", "--Credentials", dest="creds",
                    help="Credentials to download the build in this form: <user>:<password>",
                    metavar="CREDENTIALS")
  parser.add_option("-a", "--AusParameter", dest="aus",
                    help="The AUS parameter for the AUS URI (-cck param)",
                    metavar="AUS_PARAM")
  # Can't wrap the next line or else it looks wierd in parser.print_help() output
  parser.add_option("-l", "--L10NFile", dest="l10nFile", help="A text file containing the language codes for this build, separated by LF",
                    metavar="L10N_FILE")
  (options, args) = parser.parse_args()

  # Print Help if no args passed
  if not options.branch or not options.version or not options.url or \
     not options.l10nFile or not options.partner or not options.minDir:
    print "Required Items Not Specified. Must specify partner name, minotaur dir,",
    print "locale file, branch, url, and version"
    parser.print_help()
    sys.exit(1)

  # Call Main
  main(options.branch, options.version, options.url, options.partner,
       options.verificationFiles, options.minDir, options.extName,
       options.creds, options.aus, options.l10nFile)
