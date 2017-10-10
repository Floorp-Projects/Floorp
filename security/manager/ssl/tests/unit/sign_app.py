#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Given a directory of files, packages them up and signs the
resulting zip file. Mainly for creating test inputs to the
nsIX509CertDB.openSignedAppFileAsync API.
"""
from base64 import b64encode
from hashlib import sha1
import StringIO
import argparse
import os
import pycms
import re
import zipfile

def walkDirectory(directory):
    """Given a relative path to a directory, enumerates the
    files in the tree rooted at that location. Returns a list
    of pairs of paths to those files. The first in each pair
    is the full path to the file. The second in each pair is
    the path to the file relative to the directory itself."""
    paths = []
    for path, dirs, files in os.walk(directory):
        for f in files:
            fullPath = os.path.join(path, f)
            internalPath = re.sub(r'^/', '', fullPath.replace(directory, ''))
            paths.append((fullPath, internalPath))
    return paths

def signZip(appDirectory, outputFile, issuerName, doSign):
    """Given a directory containing the files to package up,
    an output filename to write to, the name of the issuer of
    the signing certificate, and whether or not to actually
    sign the resulting package, packages up the files in the
    directory and creates the output as appropriate."""
    mfEntries = []

    with zipfile.ZipFile(outputFile, 'w') as outZip:
        for (fullPath, internalPath) in walkDirectory(appDirectory):
            with open(fullPath) as inputFile:
                contents = inputFile.read()
            outZip.writestr(internalPath, contents)

            # Add the entry to the manifest we're building
            base64hash = b64encode(sha1(contents).digest())
            mfEntries.append(
                'Name: %s\nSHA1-Digest: %s\n' % (internalPath, base64hash))

        # Just exit early if we're not actually signing.
        if not doSign:
            return

        mfContents = 'Manifest-Version: 1.0\n\n' + '\n'.join(mfEntries)
        base64hash = b64encode(sha1(mfContents).digest())
        sfContents = 'Signature-Version: 1.0\nSHA1-Digest-Manifest: %s\n' % base64hash
        cmsSpecification = 'hash:%s\nsigner:\n' % sha1(sfContents).hexdigest() + \
            'issuer:%s\n' % issuerName + \
            'subject:xpcshell signed app test signer\n' + \
            'extension:keyUsage:digitalSignature'
        cmsSpecificationStream = StringIO.StringIO()
        print >>cmsSpecificationStream, cmsSpecification
        cmsSpecificationStream.seek(0)
        cms = pycms.CMS(cmsSpecificationStream)
        p7 = cms.toDER()
        outZip.writestr('META-INF/A.RSA', p7)
        outZip.writestr('META-INF/A.SF', sfContents)
        outZip.writestr('META-INF/MANIFEST.MF', mfContents)

def main(outputFile, appPath, *args):
    """Main entrypoint. Given an already-opened file-like
    object, a path to the app directory to sign, and some
    optional arguments, signs the contents of the directory and
    writes the resulting package to the 'file'."""
    parser = argparse.ArgumentParser(description='Sign an app.')
    parser.add_argument('-n', '--no-sign', action='store_true',
                        help='Don\'t actually sign - only create zip')
    parser.add_argument('-i', '--issuer', action='store', help='Issuer name',
                        default='xpcshell signed apps test root')
    parsed = parser.parse_args(args)
    signZip(appPath, outputFile, parsed.issuer, not parsed.no_sign)
