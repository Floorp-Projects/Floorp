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
from hashlib import sha1, sha256
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

def signZip(appDirectory, outputFile, issuerName, manifestHashes,
            signatureHashes, doSign):
    """Given a directory containing the files to package up,
    an output filename to write to, the name of the issuer of
    the signing certificate, a list of hash algorithms to use in
    the manifest file, a similar list for the signature file,
    and whether or not to actually sign the resulting package,
    packages up the files in the directory and creates the
    output as appropriate."""
    mfEntries = []

    with zipfile.ZipFile(outputFile, 'w') as outZip:
        for (fullPath, internalPath) in walkDirectory(appDirectory):
            with open(fullPath) as inputFile:
                contents = inputFile.read()
            outZip.writestr(internalPath, contents)

            # Add the entry to the manifest we're building
            mfEntry = 'Name: %s\n' % internalPath
            for (hashFunc, name) in manifestHashes:
                base64hash = b64encode(hashFunc(contents).digest())
                mfEntry += '%s-Digest: %s\n' % (name, base64hash)
            mfEntries.append(mfEntry)

        # Just exit early if we're not actually signing.
        if not doSign:
            return

        mfContents = 'Manifest-Version: 1.0\n\n' + '\n'.join(mfEntries)
        sfContents = 'Signature-Version: 1.0\n'
        for (hashFunc, name) in signatureHashes:
            base64hash = b64encode(hashFunc(mfContents).digest())
            sfContents += '%s-Digest-Manifest: %s\n' % (name, base64hash)

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

class Error(Exception):
    """Base class for exceptions in this module."""
    pass


class UnknownHashAlgorithmError(Error):
    """Helper exception type to handle unknown hash algorithms."""

    def __init__(self, name):
        super(UnknownHashAlgorithmError, self).__init__()
        self.name = name

    def __str__(self):
        return 'Unknown hash algorithm %s' % repr(self.name)


def hashNameToFunctionAndIdentifier(name):
    if name == 'sha1':
        return (sha1, 'SHA1')
    if name == 'sha256':
        return (sha256, 'SHA256')
    raise UnknownHashAlgorithmError(name)

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
    parser.add_argument('-m', '--manifest-hash', action='append',
                        help='Hash algorithms to use in manifest',
                        default=[])
    parser.add_argument('-s', '--signature-hash', action='append',
                        help='Hash algorithms to use in signature file',
                        default=[])
    parsed = parser.parse_args(args)
    if len(parsed.manifest_hash) == 0:
        parsed.manifest_hash.append('sha256')
    if len(parsed.signature_hash) == 0:
        parsed.signature_hash.append('sha256')
    signZip(appPath, outputFile, parsed.issuer,
            map(hashNameToFunctionAndIdentifier, parsed.manifest_hash),
            map(hashNameToFunctionAndIdentifier, parsed.signature_hash),
            not parsed.no_sign)
