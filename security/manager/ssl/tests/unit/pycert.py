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

Where:
  [] indicates an optional field or component of a field
  <> indicates a required component of a field
  {} indicates choice among a set of values
  [a,b,c] indicates a list of potential values, of which more than one
          may be used

For instance, the version field is optional. However, if it is
specified, it must have exactly one value from the set {1,2,3,4}.

In the future it will be possible to specify other properties of the
generated certificate (for example, its validity period, signature
algorithm, etc.). For now, those fields have reasonable default values.
Currently one shared RSA key is used for all signatures and subject
public key information fields. Specifying "issuerKey:alternate" or
"subjectKey:alternate" causes a different RSA key be used for signing
or as the subject public key information field, respectively.
"""

from pyasn1.codec.der import decoder
from pyasn1.codec.der import encoder
from pyasn1.type import constraint, namedtype, tag, univ, useful
from pyasn1_modules import rfc2459
import base64
import binascii
import datetime
import hashlib
import sys
import rsa

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


class UnknownKeySpecificationError(UnknownBaseError):
    """Helper exception type to handle unknown key specifications."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'key specification'


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
    commonName.setComponentByName('utf8String', string)
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

def byteStringToHexifiedBitString(string):
    """Takes a string of bytes and returns a hex string representing
    those bytes for use with pyasn1.type.univ.BitString. It must be of
    the form "'<hex bytes>'H", where the trailing 'H' indicates to
    pyasn1 that the input is a hex string."""
    return "'%s'H" % binascii.hexlify(string)

class RSAPublicKey(univ.Sequence):
    """Helper type for encoding an RSA public key"""
    componentType = namedtype.NamedTypes(
        namedtype.NamedType('N', univ.Integer()),
        namedtype.NamedType('E', univ.Integer()))


class Certificate:
    """Utility class for reading a certificate specification and
    generating a signed x509 certificate"""

    # For reference, when encoded as a subject public key info, the
    # base64-encoded sha-256 hash of this key is
    # VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=
    sharedRSA_N = long(
        '00ba8851a8448e16d641fd6eb6880636103d3c13d9eae4354ab4ecf56857'
        '6c247bc1c725a8e0d81fbdb19c069b6e1a86f26be2af5a756b6a6471087a'
        'a55aa74587f71cd5249c027ecd43fc1e69d038202993ab20c349e4dbb94c'
        'c26b6c0eed15820ff17ead691ab1d3023a8b2a41eea770e00f0d8dfd660b'
        '2bb02492a47db988617990b157903dd23bc5e0b8481fa837d38843ef2716'
        'd855b7665aaa7e02902f3a7b10800624cc1c6c97ad96615bb7e29612c075'
        '31a30c91ddb4caf7fcad1d25d309efb9170ea768e1b37b2f226f69e3b48a'
        '95611dee26d6259dab91084e36cb1c24042cbf168b2fe5f18f991731b8b3'
        'fe4923fa7251c431d503acda180a35ed8d', 16)
    sharedRSA_E = 65537L
    sharedRSA_D = long(
        '009ecbce3861a454ecb1e0fe8f85dd43c92f5825ce2e997884d0e1a949da'
        'a2c5ac559b240450e5ac9fe0c3e31c0eefa6525a65f0c22194004ee1ab46'
        '3dde9ee82287cc93e746a91929c5e6ac3d88753f6c25ba5979e73e5d8fb2'
        '39111a3cdab8a4b0cdf5f9cab05f1233a38335c64b5560525e7e3b92ad7c'
        '7504cf1dc7cb005788afcbe1e8f95df7402a151530d5808346864eb370aa'
        '79956a587862cb533791307f70d91c96d22d001a69009b923c683388c9f3'
        '6cb9b5ebe64302041c78d908206b87009cb8cabacad3dbdb2792fb911b2c'
        'f4db6603585be9ae0ca3b8e6417aa04b06e470ea1a3b581ca03a6781c931'
        '5b62b30e6011f224725946eec57c6d9441', 16)
    sharedRSA_P = long(
        '00dd6e1d4fffebf68d889c4d114cdaaa9caa63a59374286c8a5c29a717bb'
        'a60375644d5caa674c4b8bc7326358646220e4550d7608ac27d55b6db74f'
        '8d8127ef8fa09098b69147de065573447e183d22fe7d885aceb513d9581d'
        'd5e07c1a90f5ce0879de131371ecefc9ce72e9c43dc127d238190de81177'
        '3ca5d19301f48c742b', 16)
    sharedRSA_Q = long(
        '00d7a773d9ebc380a767d2fec0934ad4e8b5667240771acdebb5ad796f47'
        '8fec4d45985efbc9532968289c8d89102fadf21f34e2dd4940eba8c09d6d'
        '1f16dcc29729774c43275e9251ddbe4909e1fd3bf1e4bedf46a39b8b3833'
        '28ef4ae3b95b92f2070af26c9e7c5c9b587fedde05e8e7d86ca57886fb16'
        '5810a77b9845bc3127', 16)
    # For reference, when encoded as a subject public key info, the
    # base64-encoded sha-256 hash of this key is
    # K+uamI+1JmrxMsBxEfGOoydEDJVMa5MY/eaTj+43Lzc=
    alternateRSA_N = long(
        '00cd6e66a71b9a104c7c5f270b5869da966a52e547f8a026eef128c4d51f'
        'a7d949b1df8a1e342c59cbad0fb6ef867427bd9e76f2e9bf0b582745c646'
        '4446db7bdd4d0f2f361da724ff206d070b3d75ad87d690fa307dcccc2ad1'
        '4283921f9621f2a564e7e9f708a98556194df12fb4b0a2f0b89f76ac7e59'
        '668285aa50f14f310b6ebd8f001d0c115393bd27f3334f67780abfe0b19e'
        '5ac3414c5b4a3819fbed39198050e1c660e44cacaf108cbe1671d5a14602'
        '6090f371b2873d419eeb6de982fb493c3d4d33fb8a12bd65f1c59a3494dd'
        'd7e1131aa45e896d817bbb28e6fd4c2323ed17a26dc8e4e49281decc641e'
        'f7b7acfe65e7c0e5212fb2a9d472902c35', 16)
    alternateRSA_E = 65537L
    alternateRSA_D = long(
        '6ae6e0946550aeda9e7e059b69ceebe90a3b490542e4545e53309bfd2c13'
        'f486dd012ea6b90fbb4aba2c4b4e29f1981c9cb1d986b9dbf56bba6b8b75'
        '4c4a3b12d65ee87a88c3ca04d9a2e2df7e84166171cecfe31c13cecb194a'
        '3b9d76c271b80b498f45b93fd0b78a2e70d8e9b26598e51bae1fdb7384a2'
        '4b99b31f9bf351d9692c00d6f05c30424be4b4de55331ac77532c3fdaf74'
        '95dbf7aef601b517ed227d0efa3de443d56d8b29e556f6be938eabf4c0e4'
        '2e2fe38bec60cba5b5ff9192b68620ee4b629b9d0b64b9a8810809813b0b'
        '04e485d97fdad2961c0982a589863643974e3900dd8a75112a0fffc59f4b'
        '24c31307901dd04a848b02db32f61a01', 16)
    alternateRSA_P = long(
        '00feeacc987c0494cb5e9550eefb9dc56f9d957022a11539dae04c6361ab'
        'd5081dce2a6aec0905450886f5bb7e56e8bd2bef37cfa16fbda5ffc268ca'
        'e0499017552c37fa4a041341d67d4d69d093d8950f50672fb085b636560e'
        '2446689474b29be7abeba358ab7bc4cde3fd065d46f762adeb5c4b54ccca'
        '651a14b498311615b1', 16)
    alternateRSA_Q = long(
        '00ce4dca3fdda86b8c800c268082446633c8aaf0f20c729878092198585b'
        'd2ed134a7bdb2c93f829f99e6e9070db6598b3113627fd87bf6bc46cb2e5'
        '121777cbea9c41e74c9c2c248931dbccb5ae8a1dccfad284784cc35b8329'
        'abc420ce95640085dbf325fa7f6a2a567d487c1ef67d07a56c6beade9404'
        'd039ba01adf328ebc5', 16)

    def __init__(self, paramStream, now=datetime.datetime.utcnow()):
        self.versionValue = 2 # a value of 2 is X509v3
        self.signature = 'sha256WithRSAEncryption'
        self.issuer = 'Default Issuer'
        oneYear = datetime.timedelta(days=365)
        self.notBefore = now - oneYear
        self.notAfter = now + oneYear
        self.subject = 'Default Subject'
        self.signatureAlgorithm = 'sha256WithRSAEncryption'
        self.extensions = None
        self.subjectRSA_N = self.sharedRSA_N
        self.subjectRSA_E = self.sharedRSA_E
        self.issuerRSA_N = self.sharedRSA_N
        self.issuerRSA_E = self.sharedRSA_E
        self.issuerRSA_D = self.sharedRSA_D
        self.issuerRSA_P = self.sharedRSA_P
        self.issuerRSA_Q = self.sharedRSA_Q
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
        else:
            raise UnknownExtensionTypeError(extensionType)

    def setupKey(self, subjectOrIssuer, value):
        if value == 'alternate':
            if subjectOrIssuer == 'subject':
                self.subjectRSA_N = self.alternateRSA_N
                self.subjectRSA_E = self.alternateRSA_E
            elif subjectOrIssuer == 'issuer':
                self.issuerRSA_N = self.alternateRSA_N
                self.issuerRSA_E = self.alternateRSA_E
                self.issuerRSA_D = self.alternateRSA_D
                self.issuerRSA_P = self.alternateRSA_P
                self.issuerRSA_Q = self.alternateRSA_Q
            else:
                raise UnknownKeyTargetError(subjectOrIssuer)
        else:
            raise UnknownKeySpecificationError(value)

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
            generalName.setComponentByName('dNSName', dNSName)
            subjectAlternativeName.setComponentByPosition(count, generalName)
            count += 1
        self.addExtension(rfc2459.id_ce_subjectAltName, subjectAlternativeName)

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

    def getSubjectPublicKey(self):
        rsaKey = RSAPublicKey()
        rsaKey.setComponentByName('N', univ.Integer(self.subjectRSA_N))
        rsaKey.setComponentByName('E', univ.Integer(self.subjectRSA_E))
        return univ.BitString(byteStringToHexifiedBitString(encoder.encode(rsaKey)))

    def getSubjectPublicKeyInfo(self):
        algorithmIdentifier = rfc2459.AlgorithmIdentifier()
        algorithmIdentifier.setComponentByName('algorithm', rfc2459.rsaEncryption)
        algorithmIdentifier.setComponentByName('parameters', univ.Null())
        spki = rfc2459.SubjectPublicKeyInfo()
        spki.setComponentByName('algorithm', algorithmIdentifier)
        spki.setComponentByName('subjectPublicKey', self.getSubjectPublicKey())
        return spki

    def toDER(self):
        tbsCertificate = rfc2459.TBSCertificate()
        tbsCertificate.setComponentByName('version', self.getVersion())
        tbsCertificate.setComponentByName('serialNumber', self.getSerialNumber())
        tbsCertificate.setComponentByName('signature', self.getSignature())
        tbsCertificate.setComponentByName('issuer', self.getIssuer())
        tbsCertificate.setComponentByName('validity', self.getValidity())
        tbsCertificate.setComponentByName('subject', self.getSubject())
        tbsCertificate.setComponentByName('subjectPublicKeyInfo', self.getSubjectPublicKeyInfo())
        if self.extensions:
            extensions = rfc2459.Extensions().subtype(
                explicitTag=tag.Tag(tag.tagClassContext, tag.tagFormatSimple, 3))
            count = 0
            for extension in self.extensions:
                extensions.setComponentByPosition(count, extension)
                count += 1
            tbsCertificate.setComponentByName('extensions', extensions)
        tbsDER = encoder.encode(tbsCertificate)
        rsaPrivateKey = rsa.PrivateKey(self.issuerRSA_N, self.issuerRSA_E, self.issuerRSA_D,
                                       self.issuerRSA_P, self.issuerRSA_Q)
        signature = rsa.sign(tbsDER, rsaPrivateKey, 'SHA-256')
        certificate = rfc2459.Certificate()
        certificate.setComponentByName('tbsCertificate', tbsCertificate)
        certificate.setComponentByName('signatureAlgorithm', self.getSignatureAlgorithm())
        certificate.setComponentByName('signatureValue', byteStringToHexifiedBitString(signature))
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


# The build harness will call this function with an output file-like
# object, a path to a file containing a specification, and the path to
# the directory containing the buildid file. This will read the
# specification and output the certificate as PEM. The purpose of the
# buildid file is to provide a single definition of 'now'. This is
# particularly important when building on OS X, where we generate
# everything twice for unified builds. During the unification step, if
# any pair of input files differ, the build system throws an error.
def main(output, inputPath, buildIDPath):
    with open(buildIDPath) as buildidFile:
        buildid = buildidFile.read().strip()
    now = datetime.datetime.strptime(buildid, '%Y%m%d%H%M%S')
    with open(inputPath) as configStream:
        output.write(Certificate(configStream, now=now).toPEM())

# When run as a standalone program, this will read a specification from
# stdin and output the certificate as PEM to stdout.
if __name__ == '__main__':
    print Certificate(sys.stdin).toPEM()
