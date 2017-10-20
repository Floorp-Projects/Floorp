#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Reads a specification from stdin and outputs a PKCS7 (CMS) message with
the desired properties.

The specification format is as follows:

hash:<hex string>
signer:
<pycert specification>

hash is the value that will be put in the messageDigest attribute in
each SignerInfo of the signerInfos field of the SignedData.
The certificate specification must come last.
Currently only SHA-1 is supported.
"""

from pyasn1.codec.der import decoder
from pyasn1.codec.der import encoder
from pyasn1.type import tag, univ
from pyasn1_modules import rfc2315, rfc2459
import StringIO
import base64
import pycert
import pykey
import sys

class Error(Exception):
    """Base class for exceptions in this module."""
    pass


class UnknownDirectiveError(Error):
    """Helper exception type to handle unknown specification
    directives."""

    def __init__(self, directive):
        super(UnknownDirectiveError, self).__init__()
        self.directive = directive

    def __str__(self):
        return 'Unknown directive %s' % repr(self.directive)


class CMS(object):
    """Utility class for reading a CMS specification and
    generating a CMS message"""

    def __init__(self, paramStream):
        self.hash = ''
        signerSpecification = StringIO.StringIO()
        readingSignerSpecification = False
        for line in paramStream.readlines():
            if readingSignerSpecification:
                print >>signerSpecification, line.strip()
            elif line.strip() == 'signer:':
                readingSignerSpecification = True
            elif line.startswith('hash:'):
                self.hash = line.strip()[len('hash:'):]
            else:
                raise UnknownDirectiveError(line.strip())
        signerSpecification.seek(0)
        self.signer = pycert.Certificate(signerSpecification)
        self.signingKey = pykey.keyFromSpecification('default')

    def buildAuthenticatedAttributes(self, value, implicitTag=None):
        """Utility function to build a pyasn1 AuthenticatedAttributes
        object. Useful because when building a SignerInfo, the
        authenticatedAttributes needs to be tagged implicitly, but when
        signing an AuthenticatedAttributes, it needs the explicit SET
        tag."""
        if implicitTag:
            authenticatedAttributes = rfc2315.Attributes().subtype(implicitTag=implicitTag)
        else:
            authenticatedAttributes = rfc2315.Attributes()
        contentTypeAttribute = rfc2315.Attribute()
        # PKCS#9 contentType
        contentTypeAttribute['type'] = univ.ObjectIdentifier('1.2.840.113549.1.9.3')
        contentTypeAttribute['values'] = univ.SetOf(rfc2459.AttributeValue())
        # PKCS#7 data
        contentTypeAttribute['values'][0] = univ.ObjectIdentifier('1.2.840.113549.1.7.1')
        authenticatedAttributes[0] = contentTypeAttribute
        hashAttribute = rfc2315.Attribute()
        # PKCS#9 messageDigest
        hashAttribute['type'] = univ.ObjectIdentifier('1.2.840.113549.1.9.4')
        hashAttribute['values'] = univ.SetOf(rfc2459.AttributeValue())
        hashAttribute['values'][0] = univ.OctetString(hexValue=value)
        authenticatedAttributes[1] = hashAttribute
        return authenticatedAttributes

    def pykeyHashToDigestAlgorithm(self, pykeyHash):
        """Given a pykey hash algorithm identifier, builds an
        AlgorithmIdentifier for use with pyasn1."""
        if pykeyHash == pykey.HASH_SHA1:
            algorithmIdentifier = rfc2459.AlgorithmIdentifier()
            algorithmIdentifier['algorithm'] = univ.ObjectIdentifier('1.3.14.3.2.26')
            algorithmIdentifier['parameters'] = univ.Null()
            return algorithmIdentifier
        raise pykey.UnknownHashAlgorithmError(pykeyHash)

    def buildSignerInfo(self, certificate, pykeyHash, digestValue):
        """Given a pyasn1 certificate, a pykey hash identifier
        and a hash value, creates a SignerInfo with the
        appropriate values."""
        signerInfo = rfc2315.SignerInfo()
        signerInfo['version'] = 1
        issuerAndSerialNumber = rfc2315.IssuerAndSerialNumber()
        issuerAndSerialNumber['issuer'] = self.signer.getIssuer()
        issuerAndSerialNumber['serialNumber'] = certificate['tbsCertificate']['serialNumber']
        signerInfo['issuerAndSerialNumber'] = issuerAndSerialNumber
        signerInfo['digestAlgorithm'] = self.pykeyHashToDigestAlgorithm(pykeyHash)
        rsa = rfc2459.AlgorithmIdentifier()
        rsa['algorithm'] = rfc2459.rsaEncryption
        rsa['parameters'] = univ.Null()
        authenticatedAttributes = self.buildAuthenticatedAttributes(digestValue,
          implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatConstructed, 0))
        authenticatedAttributesTBS = self.buildAuthenticatedAttributes(digestValue)
        signerInfo['authenticatedAttributes'] = authenticatedAttributes
        signerInfo['digestEncryptionAlgorithm'] = rsa
        authenticatedAttributesEncoded = encoder.encode(authenticatedAttributesTBS)
        signature = self.signingKey.sign(authenticatedAttributesEncoded, pykeyHash)
        # signature will be a hexified bit string of the form
        # "'<hex bytes>'H". For some reason that's what BitString wants,
        # but since this is an OCTET STRING, we have to strip off the
        # quotation marks and trailing "H".
        signerInfo['encryptedDigest'] = univ.OctetString(hexValue=signature[1:-2])
        return signerInfo

    def toDER(self):
        contentInfo = rfc2315.ContentInfo()
        contentInfo['contentType'] = rfc2315.signedData

        signedData = rfc2315.SignedData()
        signedData['version'] = rfc2315.Version(1)

        digestAlgorithms = rfc2315.DigestAlgorithmIdentifiers()
        digestAlgorithms[0] = self.pykeyHashToDigestAlgorithm(pykey.HASH_SHA1)
        signedData['digestAlgorithms'] = digestAlgorithms

        dataContentInfo = rfc2315.ContentInfo()
        dataContentInfo['contentType'] = rfc2315.data
        signedData['contentInfo'] = dataContentInfo

        certificates = rfc2315.ExtendedCertificatesAndCertificates().subtype(
            implicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatConstructed, 0))
        extendedCertificateOrCertificate = rfc2315.ExtendedCertificateOrCertificate()
        certificate = decoder.decode(self.signer.toDER(),
            asn1Spec=rfc2459.Certificate())[0]
        extendedCertificateOrCertificate['certificate'] = certificate
        certificates[0] = extendedCertificateOrCertificate
        signedData['certificates'] = certificates

        signerInfos = rfc2315.SignerInfos()

        signerInfos[0] = self.buildSignerInfo(certificate, pykey.HASH_SHA1, self.hash)
        signedData['signerInfos'] = signerInfos

        encoded = encoder.encode(signedData)
        anyTag = univ.Any(encoded).subtype(
            explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatConstructed, 0))

        contentInfo['content'] = anyTag
        return encoder.encode(contentInfo)

    def toPEM(self):
        output = '-----BEGIN PKCS7-----'
        der = self.toDER()
        b64 = base64.b64encode(der)
        while b64:
            output += '\n' + b64[:64]
            b64 = b64[64:]
        output += '\n-----END PKCS7-----'
        return output


# When run as a standalone program, this will read a specification from
# stdin and output the certificate as PEM to stdout.
if __name__ == '__main__':
    print CMS(sys.stdin).toPEM()
