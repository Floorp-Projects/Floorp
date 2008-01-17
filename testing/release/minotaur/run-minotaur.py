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
# Portions created by the Initial Developer are Copyright (C) 2008
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
import partner


def main(branch, version, url, loc, name, vFiles, minDir, creds, aus):
  partner.doDownload(name, loc, url, minDir, creds)
  partner.doInstall(branch, name, loc)
  partner.doMinotaur(name, loc, minDir, vFiles, aus, version)
  partner.doUninstall(branch, name, loc)

if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-n", "--Name", dest="name", help="Build Name",
                    metavar="PARTNER_NAME")
  parser.add_option("-b", "--Branch", dest="branch",
                   help="Gecko Branch: 1.8.0|1.8.1|1.9", metavar="BRANCH")
  parser.add_option("-v", "--Version ", dest="version",
                    help="version of firefox to be tested",
                    metavar="FIREFOX_VERSION")
  parser.add_option("-u", "--UrlToBuild", dest="url",
                    help="URL to top level build location, above the OS directories",
                    metavar="URL")
  parser.add_option("-l", "--Locale", dest="loc", help="Locale for this build",
                    metavar="LOCALE")
  parser.add_option("-f", "--VerificationFileLocation", dest="verificationFiles",
                    help="location of verification files, leave blank to create verification files",
                    metavar="VER_FILES")
  parser.add_option("-m", "--MinotaurDirectory", dest="minDir",
                    help="Directory of the Minotuar code",
                    metavar="MINOTAUR_DIR")
  parser.add_option("-c", "--Credentials", dest="creds",
                    help="Credentials to download the build in this form: <user>:<password>",
                    metavar="CREDENTIALS")
  parser.add_option("-a", "--AusParameter", dest="aus",
                    help="The AUS parameter for the AUS URI (-cck param)",
                    metavar="AUS_PARAM")
  (options, args) = parser.parse_args()

  # Call Main
  main(options.branch, options.version, options.url, options.loc, options.name,
       options.verificationFiles, options.minDir, options.creds, options.aus)
