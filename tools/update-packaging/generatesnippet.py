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
# The Original Code is the Mozilla build system
#
# The Initial Developer of the Original Code is Mozilla.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#    Armen Zambrano Gasparnian <armenzg@mozilla.com>
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

"""
This script generates the complete snippet for a given locale or en-US
Most of the parameters received are to generate the MAR's download URL
and determine the MAR's filename
"""
import sys, os, platform, sha
from optparse import OptionParser
from ConfigParser import ConfigParser
from stat import ST_SIZE

def main():
    error = False
    parser = OptionParser(
        usage="%prog [options]")
    parser.add_option("--mar-path",
                      action="store",
                      dest="marPath",
                      help="[Required] Specify the absolute path where the MAR file is found.")
    parser.add_option("--application-ini-file",
                      action="store",
                      dest="applicationIniFile",
                      help="[Required] Specify the absolute path to the application.ini file.")
    parser.add_option("-l",
                      "--locale",
                      action="store",
                      dest="locale",
                      help="[Required] Specify which locale we are generating the snippet for.")
    parser.add_option("-p",
                      "--product",
                      action="store",
                      dest="product",
                      help="[Required] This option is used to generate the URL to download the MAR file.")
    parser.add_option("--platform",
                      action="store",
                      dest="platform",
                      help="[Required] This option is used to indicate which target platform.")
    parser.add_option("--branch",
                      action="store",
                      dest="branch",
                      help="This option is used to indicate which branch name to use for FTP file names.")
    parser.add_option("--download-base-URL",
                      action="store",
                      dest="downloadBaseURL",
                      help="This option indicates under which.")
    parser.add_option("-v",
                      "--verbose",
                      action="store_true",
                      dest="verbose",
                      default=False,
                      help="This option increases the output of the script.")
    (options, args) = parser.parse_args()
    for req, msg in (('marPath', "the absolute path to the where the MAR file is"),
                     ('applicationIniFile', "the absolute path to the application.ini file."),
                     ('locale', "a locale."),
                     ('product', "specify a product."),
                     ('platform', "specify the platform.")):
        if not hasattr(options, req):
            parser.error('You must specify %s' % msg)

    if not options.downloadBaseURL or options.downloadBaseURL == '':
        options.downloadBaseURL = 'http://ftp.mozilla.org/pub/mozilla.org/%s/nightly' % options.product

    if not options.branch or options.branch == '':
        options.branch = None

    snippet = generateSnippet(options.marPath,
                              options.applicationIniFile,
                              options.locale,
                              options.downloadBaseURL,
                              options.product,
                              options.platform,
                              options.branch)
    f = open(os.path.join(options.marPath, 'complete.update.snippet'), 'wb')
    f.write(snippet)
    f.close()

    if options.verbose:
        # Show in our logs what the contents of the snippet are
        print snippet

def generateSnippet(abstDistDir, applicationIniFile, locale,
                    downloadBaseURL, product, platform, branch):
    # Let's extract information from application.ini
    c = ConfigParser()
    try:
        c.readfp(open(applicationIniFile))
    except IOError, (stderror):
       sys.exit(stderror)
    buildid = c.get("App", "BuildID")
    appVersion = c.get("App", "Version")
    branchName = branch or c.get("App", "SourceRepository").split('/')[-1]

    marFileName = '%s-%s.%s.%s.complete.mar' % (
        product,
        appVersion,
        locale,
        platform)
    # Let's determine the hash and the size of the MAR file
    # This function exits the script if the file does not exist
    (completeMarHash, completeMarSize) = getFileHashAndSize(
        os.path.join(abstDistDir, marFileName))
    # Construct the URL to where the MAR file will exist
    interfix = ''
    if locale == 'en-US':
        interfix = ''
    else:
        interfix = '-l10n'
    marDownloadURL = "%s/%s%s/%s" % (downloadBaseURL,
                                     datedDirPath(buildid, branchName),
                                     interfix,
                                     marFileName)

    snippet = """complete
%(marDownloadURL)s
sha1
%(completeMarHash)s
%(completeMarSize)s
%(buildid)s
%(appVersion)s
%(appVersion)s
""" % dict( marDownloadURL=marDownloadURL,
            completeMarHash=completeMarHash,
            completeMarSize=completeMarSize,
            buildid=buildid,
            appVersion=appVersion)

    return snippet

def getFileHashAndSize(filepath):
    sha1Hash = 'UNKNOWN'
    size = 'UNKNOWN'

    try:
        # open in binary mode to make sure we get consistent results
        # across all platforms
        f = open(filepath, "rb")
        shaObj = sha.new(f.read())
        sha1Hash = shaObj.hexdigest()
        size = os.stat(filepath)[ST_SIZE]
    except IOError, (stderror):
       sys.exit(stderror)

    return (sha1Hash, size)

def datedDirPath(buildid, milestone):
    """
    Returns a string that will look like:
    2009/12/2009-12-31-09-mozilla-central
    """
    year  = buildid[0:4]
    month = buildid[4:6]
    day   = buildid[6:8]
    hour  = buildid[8:10]
    datedDir = "%s-%s-%s-%s-%s" % (year,
                                   month,
                                   day,
                                   hour,
                                   milestone)
    return "%s/%s/%s" % (year, month, datedDir)

if __name__ == '__main__':
    main()
