#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Reads a certificate specification from stdin or a file and outputs a
signed x509 certificate with the desired properties.

The input format is as follows:

issuer:<string to use as the issuer common name>
subject:<string to use as the subject common name>
[version:<{1,2,3,4}>]
[validity:<YYYYMMDD-YYYYMMDD|duration in days>]
[issuerKey:alternate]
[subjectKey:alternate]
[extension:<extension name:<extension-specific data>>]
[...]

Known extensions are:
basicConstraints:[cA],[pathLenConstraint]
keyUsage:[digitalSignature,nonRepudiation,keyEncipherment,
          dataEncipherment,keyAgreement,keyCertSign,cRLSign]
extKeyUsage:[serverAuth,clientAuth,codeSigning,emailProtection
             nsSGC, # Netscape Server Gated Crypto
             OCSPSigning,timeStamping]
subjectAlternativeName:[<dNSName>,...]
authorityInformationAccess:<OCSP URI>
certificatePolicies:<policy OID>

Where:
  [] indicates an optional field or component of a field
  <> indicates a required component of a field
  {} indicates choice among a set of values
  [a,b,c] indicates a list of potential values, of which more than one
          may be used

For instance, the version field is optional. However, if it is
specified, it must have exactly one value from the set {1,2,3,4}.

In the future it will be possible to specify other properties of the
generated certificate (for example, the signature algorithm). For now,
those fields have reasonable default values. Currently one shared RSA
key is used for all signatures and subject public key information
fields. Specifying "issuerKey:alternate" or "subjectKey:alternate"
causes a different RSA key be used for signing or as the subject public
key information field, respectively. Other keys are also available -
see pykey.py.

The validity period may be specified as either concrete notBefore and
notAfter values or as a validity period centered around 'now'. For the
latter, this will result in a notBefore of 'now' - duration/2 and a
notAfter of 'now' + duration/2.
"""

from pyasn1.codec.der import decoder
from pyasn1.codec.der import encoder
from pyasn1.type import constraint, tag, univ, useful
from pyasn1_modules import rfc2459
import base64
import datetime
import hashlib
import re
import sys

import pykey

class UnknownBaseError(Exception):
    """Base class for handling unexpected input in this module."""
    def __init__(self, value):
        self.value = value
        self.category = 'input'

    def __str__(self):
        return 'Unknown %s type "%s"' % (self.category, repr(self.value))


class UnknownAlgorithmTypeError(UnknownBaseError):
    """Helper exception type to handle unknown algorithm types."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'algorithm'


class UnknownParameterTypeError(UnknownBaseError):
    """Helper exception type to handle unknown input parameters."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'parameter'


class UnknownExtensionTypeError(UnknownBaseError):
    """Helper exception type to handle unknown input extensions."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'extension'


class UnknownKeyPurposeTypeError(UnknownBaseError):
    """Helper exception type to handle unknown key purposes."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'keyPurpose'


class UnknownKeyTargetError(UnknownBaseError):
    """Helper exception type to handle unknown key targets."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'key target'


class UnknownVersionError(UnknownBaseError):
    """Helper exception type to handle unknown specified versions."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'version'


def getASN1Tag(asn1Type):
    """Helper function for returning the base tag value of a given
    type from the pyasn1 package"""
    return asn1Type.baseTagSet.getBaseTag().asTuple()[2]

def stringToAccessDescription(string):
    """Helper function that takes a string representing a URI
    presumably identifying an OCSP authority information access
    location. Returns an AccessDescription usable by pyasn1."""
    accessMethod = rfc2459.id_ad_ocsp
    accessLocation = rfc2459.GeneralName()
    accessLocation.setComponentByName('uniformResourceIdentifier', string)
    sequence = univ.Sequence()
    sequence.setComponentByPosition(0, accessMethod)
    sequence.setComponentByPosition(1, accessLocation)
    return sequence

def stringToAlgorithmIdentifier(string):
    """Helper function that converts a description of an algorithm
    to a representation usable by the pyasn1 package"""
    algorithmIdentifier = rfc2459.AlgorithmIdentifier()
    algorithm = None
    if string == 'sha256WithRSAEncryption':
        algorithm = univ.ObjectIdentifier('1.2.840.113549.1.1.11')
    # In the future, more algorithms will be supported.
    if algorithm == None:
        raise UnknownAlgorithmTypeError(string)
    algorithmIdentifier.setComponentByName('algorithm', algorithm)
    return algorithmIdentifier

def stringToCommonName(string):
    """Helper function for taking a string and building an x520 name
    representation usable by the pyasn1 package. Currently returns one
    RDN with one AVA consisting of a Common Name encoded as a
    UTF8String."""
    commonName = rfc2459.X520CommonName()
    # The string may have things like '\0' (i.e. a slash followed by
    # the number zero) that have to be decoded into the resulting
    # '\x00' (i.e. a byte with value zero).
    commonName.setComponentByName('utf8String', string.decode(encoding='string_escape'))
    ava = rfc2459.AttributeTypeAndValue()
    ava.setComponentByName('type', rfc2459.id_at_commonName)
    ava.setComponentByName('value', commonName)
    rdn = rfc2459.RelativeDistinguishedName()
    rdn.setComponentByPosition(0, ava)
    rdns = rfc2459.RDNSequence()
    rdns.setComponentByPosition(0, rdn)
    name = rfc2459.Name()
    name.setComponentByPosition(0, rdns)
    return name

def datetimeToTime(dt):
    """Takes a datetime object and returns an rfc2459.Time object with
    that time as its value as a GeneralizedTime"""
    time = rfc2459.Time()
    time.setComponentByName('generalTime', useful.GeneralizedTime(dt.strftime('%Y%m%d%H%M%SZ')))
    return time

class Certificate:
    """Utility class for reading a certificate specification and
    generating a signed x509 certificate"""

    def __init__(self, paramStream):
        self.versionValue = 2 # a value of 2 is X509v3
        self.signature = 'sha256WithRSAEncryption'
        self.issuer = 'Default Issuer'
        actualNow = datetime.datetime.utcnow()
        self.now = datetime.datetime.strptime(str(actualNow.year), '%Y')
        aYearAndAWhile = datetime.timedelta(days=550)
        self.notBefore = self.now - aYearAndAWhile
        self.notAfter = self.now + aYearAndAWhile
        self.subject = 'Default Subject'
        self.signatureAlgorithm = 'sha256WithRSAEncryption'
        self.extensions = None
        self.subjectKey = pykey.RSAKey()
        self.issuerKey = pykey.RSAKey()
        self.decodeParams(paramStream)
        self.serialNumber = self.generateSerialNumber()

    def generateSerialNumber(self):
        """Generates a serial number for this certificate based on its
        contents. Intended to be reproducible for compatibility with
        the build system on OS X (see the comment above main, later in
        this file)."""
        hasher = hashlib.sha256()
        hasher.update(str(self.versionValue))
        hasher.update(self.signature)
        hasher.update(self.issuer)
        hasher.update(str(self.notBefore))
        hasher.update(str(self.notAfter))
        hasher.update(self.subject)
        hasher.update(self.signatureAlgorithm)
        if self.extensions:
            for extension in self.extensions:
                hasher.update(str(extension))
        serialBytes = [ord(c) for c in hasher.digest()[:20]]
        # Ensure that the most significant bit isn't set (which would
        # indicate a negative number, which isn't valid for serial
        # numbers).
        serialBytes[0] &= 0x7f
        # Also ensure that the least significant bit on the most
        # significant byte is set (to prevent a leading zero byte,
        # which also wouldn't be valid).
        serialBytes[0] |= 0x01
        # Now prepend the ASN.1 INTEGER tag and length bytes.
        serialBytes.insert(0, len(serialBytes))
        serialBytes.insert(0, getASN1Tag(univ.Integer))
        return ''.join(chr(b) for b in serialBytes)

    def decodeParams(self, paramStream):
        for line in paramStream.readlines():
            self.decodeParam(line.strip())

    def decodeParam(self, line):
        param = line.split(':')[0]
        value = ':'.join(line.split(':')[1:])
        if param == 'version':
            self.setVersion(value)
        elif param == 'subject':
            self.subject = value
        elif param == 'issuer':
            self.issuer = value
        elif param == 'validity':
            self.decodeValidity(value)
        elif param == 'extension':
            self.decodeExtension(value)
        elif param == 'issuerKey':
            self.setupKey('issuer', value)
        elif param == 'subjectKey':
            self.setupKey('subject', value)
        else:
            raise UnknownParameterTypeError(param)

    def setVersion(self, version):
        intVersion = int(version)
        if intVersion >= 1 and intVersion <= 4:
            self.versionValue = intVersion - 1
        else:
            raise UnknownVersionError(version)

    def decodeValidity(self, duration):
        match = re.search('([0-9]{8})-([0-9]{8})', duration)
        if match:
            self.notBefore = datetime.datetime.strptime(match.group(1), '%Y%m%d')
            self.notAfter = datetime.datetime.strptime(match.group(2), '%Y%m%d')
        else:
            delta = datetime.timedelta(days=(int(duration) / 2))
            self.notBefore = self.now - delta
            self.notAfter = self.now + delta

    def decodeExtension(self, extension):
        extensionType = extension.split(':')[0]
        value = ':'.join(extension.split(':')[1:])
        if extensionType == 'basicConstraints':
            self.addBasicConstraints(value)
        elif extensionType == 'keyUsage':
            self.addKeyUsage(value)
        elif extensionType == 'extKeyUsage':
            self.addExtKeyUsage(value)
        elif extensionType == 'subjectAlternativeName':
            self.addSubjectAlternativeName(value)
        elif extensionType == 'authorityInformationAccess':
            self.addAuthorityInformationAccess(value)
        elif extensionType == 'certificatePolicies':
            self.addCertificatePolicies(value)
        else:
            raise UnknownExtensionTypeError(extensionType)

    def setupKey(self, subjectOrIssuer, value):
        if subjectOrIssuer == 'subject':
            self.subjectKey = pykey.RSAKey(value)
        elif subjectOrIssuer == 'issuer':
            self.issuerKey = pykey.RSAKey(value)
        else:
            raise UnknownKeyTargetError(subjectOrIssuer)

    def addExtension(self, extensionType, extensionValue):
        if not self.extensions:
            self.extensions = []
        encapsulated = univ.OctetString(encoder.encode(extensionValue))
        extension = rfc2459.Extension()
        extension.setComponentByName('extnID', extensionType)
        extension.setComponentByName('extnValue', encapsulated)
        self.extensions.append(extension)

    def addBasicConstraints(self, basicConstraints):
        cA = basicConstraints.split(',')[0]
        pathLenConstraint = basicConstraints.split(',')[1]
        basicConstraintsExtension = rfc2459.BasicConstraints()
        basicConstraintsExtension.setComponentByName('cA', cA == 'cA')
        if pathLenConstraint:
            pathLenConstraintValue = \
                univ.Integer(int(pathLenConstraint)).subtype(
                    subtypeSpec=constraint.ValueRangeConstraint(0, 64))
            basicConstraintsExtension.setComponentByName('pathLenConstraint',
                                                         pathLenConstraintValue)
        self.addExtension(rfc2459.id_ce_basicConstraints, basicConstraintsExtension)

    def addKeyUsage(self, keyUsage):
        keyUsageExtension = rfc2459.KeyUsage(keyUsage)
        self.addExtension(rfc2459.id_ce_keyUsage, keyUsageExtension)

    def keyPurposeToOID(self, keyPurpose):
        if keyPurpose == 'serverAuth':
            # the OID for id_kp_serverAuth is incorrect in the
            # pyasn1-modules implementation
            return univ.ObjectIdentifier('1.3.6.1.5.5.7.3.1')
        if keyPurpose == 'clientAuth':
            return rfc2459.id_kp_clientAuth
        if keyPurpose == 'codeSigning':
            return rfc2459.id_kp_codeSigning
        if keyPurpose == 'emailProtection':
            return rfc2459.id_kp_emailProtection
        if keyPurpose == 'nsSGC':
            return univ.ObjectIdentifier('2.16.840.1.113730.4.1')
        if keyPurpose == 'OCSPSigning':
            return univ.ObjectIdentifier('1.3.6.1.5.5.7.3.9')
        if keyPurpose == 'timeStamping':
            return rfc2459.id_kp_timeStamping
        raise UnknownKeyPurposeTypeError(keyPurpose)

    def addExtKeyUsage(self, extKeyUsage):
        extKeyUsageExtension = rfc2459.ExtKeyUsageSyntax()
        count = 0
        for keyPurpose in extKeyUsage.split(','):
            extKeyUsageExtension.setComponentByPosition(count, self.keyPurposeToOID(keyPurpose))
            count += 1
        self.addExtension(rfc2459.id_ce_extKeyUsage, extKeyUsageExtension)

    def addSubjectAlternativeName(self, dNSNames):
        subjectAlternativeName = rfc2459.SubjectAltName()
        count = 0
        for dNSName in dNSNames.split(','):
            generalName = rfc2459.GeneralName()
            # The string may have things like '\0' (i.e. a slash
            # followed by the number zero) that have to be decoded into
            # the resulting '\x00' (i.e. a byte with value zero).
            generalName.setComponentByName('dNSName', dNSName.decode(encoding='string_escape'))
            subjectAlternativeName.setComponentByPosition(count, generalName)
            count += 1
        self.addExtension(rfc2459.id_ce_subjectAltName, subjectAlternativeName)

    def addAuthorityInformationAccess(self, ocspURI):
        sequence = univ.Sequence()
        accessDescription = stringToAccessDescription(ocspURI)
        sequence.setComponentByPosition(0, accessDescription)
        self.addExtension(rfc2459.id_pe_authorityInfoAccess, sequence)

    def addCertificatePolicies(self, policyOID):
        policies = rfc2459.CertificatePolicies()
        policy = rfc2459.PolicyInformation()
        if policyOID == 'any':
            policyOID = '2.5.29.32.0'
        policyIdentifier = rfc2459.CertPolicyId(policyOID)
        policy.setComponentByName('policyIdentifier', policyIdentifier)
        policies.setComponentByPosition(0, policy)
        self.addExtension(rfc2459.id_ce_certificatePolicies, policies)

    def getVersion(self):
        return rfc2459.Version(self.versionValue).subtype(
            explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 0))

    def getSerialNumber(self):
        return decoder.decode(self.serialNumber)[0]

    def getSignature(self):
        return stringToAlgorithmIdentifier(self.signature)

    def getIssuer(self):
        return stringToCommonName(self.issuer)

    def getValidity(self):
        validity = rfc2459.Validity()
        validity.setComponentByName('notBefore', self.getNotBefore())
        validity.setComponentByName('notAfter', self.getNotAfter())
        return validity

    def getNotBefore(self):
        return datetimeToTime(self.notBefore)

    def getNotAfter(self):
        return datetimeToTime(self.notAfter)

    def getSubject(self):
        return stringToCommonName(self.subject)

    def getSignatureAlgorithm(self):
        return stringToAlgorithmIdentifier(self.signature)

    def toDER(self):
        tbsCertificate = rfc2459.TBSCertificate()
        tbsCertificate.setComponentByName('version', self.getVersion())
        tbsCertificate.setComponentByName('serialNumber', self.getSerialNumber())
        tbsCertificate.setComponentByName('signature', self.getSignature())
        tbsCertificate.setComponentByName('issuer', self.getIssuer())
        tbsCertificate.setComponentByName('validity', self.getValidity())
        tbsCertificate.setComponentByName('subject', self.getSubject())
        tbsCertificate.setComponentByName('subjectPublicKeyInfo',
                                          self.subjectKey.asSubjectPublicKeyInfo())
        if self.extensions:
            extensions = rfc2459.Extensions().subtype(
                explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 3))
            count = 0
            for extension in self.extensions:
                extensions.setComponentByPosition(count, extension)
                count += 1
            tbsCertificate.setComponentByName('extensions', extensions)
        certificate = rfc2459.Certificate()
        certificate.setComponentByName('tbsCertificate', tbsCertificate)
        certificate.setComponentByName('signatureAlgorithm', self.getSignatureAlgorithm())
        tbsDER = encoder.encode(tbsCertificate)
        certificate.setComponentByName('signatureValue', self.issuerKey.sign(tbsDER))
        return encoder.encode(certificate)

    def toPEM(self):
        output = '-----BEGIN CERTIFICATE-----'
        der = self.toDER()
        b64 = base64.b64encode(der)
        while b64:
            output += '\n' + b64[:64]
            b64 = b64[64:]
        output += '\n-----END CERTIFICATE-----'
        return output


# The build harness will call this function with an output
# file-like object and a path to a file containing a
# specification. This will read the specification and output
# the certificate as PEM.
# This utility tries as hard as possible to ensure that two
# runs with the same input will have the same output. This is
# particularly important when building on OS X, where we
# generate everything twice for unified builds. During the
# unification step, if any pair of input files differ, the build
# system throws an error.
# The one concrete failure mode is if one run happens before
# midnight on New Year's Eve and the next run happens after
# midnight.
def main(output, inputPath):
    with open(inputPath) as configStream:
        output.write(Certificate(configStream).toPEM())

# When run as a standalone program, this will read a specification from
# stdin and output the certificate as PEM to stdout.
if __name__ == '__main__':
    print Certificate(sys.stdin).toPEM()
