#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Provides methods for signing data and representing a key as a
subject public key info for use with pyasn1.

The key specification format is currently very simple. If it is
empty, one RSA key is used. If it consists of the string
'alternate', a different RSA key is used. In the future it will
be possible to specify other properties of the key (type,
strength, signature algorithm, etc.).
"""

from pyasn1.codec.der import encoder
from pyasn1.type import univ, namedtype
from pyasn1_modules import rfc2459
import binascii
import rsa

def byteStringToHexifiedBitString(string):
    """Takes a string of bytes and returns a hex string representing
    those bytes for use with pyasn1.type.univ.BitString. It must be of
    the form "'<hex bytes>'H", where the trailing 'H' indicates to
    pyasn1 that the input is a hex string."""
    return "'%s'H" % binascii.hexlify(string)

class UnknownBaseError(Exception):
    """Base class for handling unexpected input in this module."""
    def __init__(self, value):
        self.value = value
        self.category = 'input'

    def __str__(self):
        return 'Unknown %s type "%s"' % (self.category, repr(self.value))


class UnknownKeySpecificationError(UnknownBaseError):
    """Helper exception type to handle unknown key specifications."""

    def __init__(self, value):
        UnknownBaseError.__init__(self, value)
        self.category = 'key specification'


class RSAPublicKey(univ.Sequence):
    """Helper type for encoding an RSA public key"""
    componentType = namedtype.NamedTypes(
        namedtype.NamedType('N', univ.Integer()),
        namedtype.NamedType('E', univ.Integer()))


class RSAKey:
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
    sharedRSA_exp1 = long(
        '0096472b41a610c0ade1af2266c1600e3671355ba42d4b5a0eb4e9d7eb35'
        '81400ba5dd132cdb1a5e9328c7bbc0bbb0155ea192972edf97d12751d8fc'
        'f6ae572a30b1ea309a8712dd4e33241db1ee455fc093f5bc9b592d756e66'
        '21474f32c07af22fb275d340792b32ba2590bbb261aefb95a258eea53765'
        '5315be9c24d191992d', 16)
    sharedRSA_exp2 = long(
        '28b450a7a75a856413b2bda6f7a63e3d964fb9ecf50e3823ef6cc8e8fa26'
        'ee413f8b9d1205540f12bbe7a0c76828b7ba65ad83cca4d0fe2a220114e1'
        'b35d03d5a85bfe2706bd50fce6cfcdd571b46ca621b8ed47d605bbe765b0'
        'aa4a0665ac25364da20154032e1204b8559d3e34fb5b177c9a56ff93510a'
        '5a4a6287c151de2d', 16)
    sharedRSA_coef = long(
        '28067b9355801d2ef52dfa96d8adb589673cf8ee8a9c6ff72aeeabe9ef6b'
        'e58a4f4abf05f788947dc851fdaa34542147a71a246bfb054ee76aa346ab'
        'cd2692cfc9e44c51e6f069c735e073ba019f6a7214961c91b26871caeabf'
        '8f064418a02690e39a8d5ff3067b7cdb7f50b1f53418a703966c4fc774bf'
        '7402af6c43247f43', 16)

    # For reference, when encoded as a subject public key info, the
    # base64-encoded sha-256 hash of this key is
    # MQj2tt1yGAfwFpWETYUCVrZxk2CD2705NKBQUlAaKJI=
    alternateRSA_N = long(
        '00c175c65266099f77082a6791f1b876c37f5ce538b06c4acd22b1cbd46f'
        'a65ada2add41c8c2498ac4a3b3c1f61487f41b698941bd80a51c3c120244'
        'c584a4c4483305e5138c0106cf08be9a862760bae6a2e8f36f23c5d98313'
        'b9dfaf378345dace51d4d6dcd2a6cb3cc706ebcd3070ec98cce40aa591d7'
        '295a7f71c5be66691d2b2dfec84944590bc5a3ea49fd93b1d753405f1773'
        '7699958666254797ed426908880811422069988a43fee48ce68781dd22b6'
        'a69cd28375131f932b128ce286fa7d251c062ad27ef016f187cdd54e832b'
        '35b8930f74ba90aa8bc76167242ab1fd6d62140d18c4c0b8c68fc3748457'
        '324ad7de86e6552f1d1e191d712168d3bb', 16)
    alternateRSA_E = 65537L
    alternateRSA_D = long(
        '7e3f6d7cb839ef66ae5d7dd92ff5410bb341dc14728d39034570e1a37079'
        '0f30f0681355fff41e2ad4e9a9d9fcebfbd127bdfab8c00affb1f3cea732'
        '7ead47aa1621f2ac1ee14ca02f04b3b2786017980b181a449d03b03e69d1'
        '12b83571e55434f012056575d2832ed6731dce799e37c83f6d51c55ab71e'
        'b58015af05e1af15c747603ef7f27d03a6ff049d96bbf854c1e4e50ef5b0'
        '58d0fb08180e0ac7f7be8f2ff1673d97fc9e55dba838077bbf8a7cff2962'
        '857785269cd9d5bad2b57469e4afcd33c4ca2d2f699f11e7c8fbdcd484f0'
        '8d8efb8a3cb8a972eb24bed972efaae4bb712093e48fe94a46eb629a8750'
        '78c4021a9a2c93c9a70390e9d0a54401', 16)
    alternateRSA_P = long(
        '00e63fc725a6ba76925a7ff8cb59c4f56dd7ec83fe85bf1f53e11cac9a81'
        '258bcfc0ae819077b0f2d1477aaf868de6a8ecbeaf7bb22b196f2a9ad82d'
        '3286f0d0cc29de719e5f2be8e509b7284d5963edd362f927887a4c4a8979'
        '9d340d51b301ac7601ab27179024fcaadd38bf6522af63eb16461ec02a7f'
        '27b06fe09ddda7c0a1', 16)
    alternateRSA_Q = long(
        '00d718b1fe9f8f99f00e832ae1fbdc6fe2ab27f34e049c498010fa0eb708'
        '4852182346083b5c96c3eee5592c014a410c6b930b165c13b5c26aa32eac'
        '6e7c925a8551c25134f2f4a72c6421f19a73148a0edfaba5d3a6888b35cb'
        'a18c00fd38ee5aaf0b545731d720761bbccdee744a52ca415e98e4de01cd'
        'fe764c1967b3e8cadb', 16)
    alternateRSA_exp1 = long(
        '01e5aca266c94a88d22e13c2b92ea247116c657a076817bdfd30db4b3a9d'
        '3095b9a4b6749647e2f84e7a784fc7838b08c85971cf7a036fa30e3b91c3'
        'c4d0df278f80c1b6e859d8456adb137defaa9f1f0ac5bac9a9184fd4ea27'
        '9d722ea626f160d78aad7bc83845ccb29df115c83f61b7622b99bd439c60'
        '9b5790a63c595181', 16)
    alternateRSA_exp2 = long(
        '0080cc45d10d2484ee0d1297fc07bf80b3beff461ea27e1f38f371789c3a'
        'f66b4a0edd2192c227791db4f1c77ae246bf342f31856b0f56581b58a95b'
        '1131c0c5396db2a8c3c6f39ea2e336bc205ae6a2a0b36869fca98cbba733'
        'cf01319a6f9bb26b7ca23d3017fc551cd8da8afdd17f6fa2e30d34868798'
        '1cd6234d571e90b7df', 16)
    alternateRSA_coef = long(
        '6f77c0c1f2ae7ac169561cca499c52bdfbe04cddccdbdc12aec5a85691e8'
        '594b7ee29908f30e7b96aa6254b80ed4aeec9b993782bdfc79b69d8d58c6'
        '8870fa4be1bc0c3527288c5c82bb4aebaf15edff110403fc78e6ace6a828'
        '27bf42f0cfa751e507651c5638db9393dd23dd1f6b295151de44b77fe55a'
        '7b0df271e19a65c0', 16)

    def __init__(self, specification = None):
        if not specification:
            self.RSA_N = self.sharedRSA_N
            self.RSA_E = self.sharedRSA_E
            self.RSA_D = self.sharedRSA_D
            self.RSA_P = self.sharedRSA_P
            self.RSA_Q = self.sharedRSA_Q
            self.RSA_exp1 = self.sharedRSA_exp1
            self.RSA_exp2 = self.sharedRSA_exp2
            self.RSA_coef = self.sharedRSA_coef
        elif specification == 'alternate':
            self.RSA_N = self.alternateRSA_N
            self.RSA_E = self.alternateRSA_E
            self.RSA_D = self.alternateRSA_D
            self.RSA_P = self.alternateRSA_P
            self.RSA_Q = self.alternateRSA_Q
            self.RSA_exp1 = self.alternateRSA_exp1
            self.RSA_exp2 = self.alternateRSA_exp2
            self.RSA_coef = self.alternateRSA_coef
        else:
            raise UnknownKeySpecificationError(specification)

    def asSubjectPublicKeyInfo(self):
        """Returns a subject public key info representing
        this key for use by pyasn1."""
        algorithmIdentifier = rfc2459.AlgorithmIdentifier()
        algorithmIdentifier.setComponentByName('algorithm', rfc2459.rsaEncryption)
        algorithmIdentifier.setComponentByName('parameters', univ.Null())
        spki = rfc2459.SubjectPublicKeyInfo()
        spki.setComponentByName('algorithm', algorithmIdentifier)
        rsaKey = RSAPublicKey()
        rsaKey.setComponentByName('N', univ.Integer(self.RSA_N))
        rsaKey.setComponentByName('E', univ.Integer(self.RSA_E))
        subjectPublicKey = univ.BitString(byteStringToHexifiedBitString(encoder.encode(rsaKey)))
        spki.setComponentByName('subjectPublicKey', subjectPublicKey)
        return spki

    def sign(self, data):
        """Returns a hexified bit string representing a
        signature by this key over the specified data.
        Intended for use with pyasn1.type.univ.BitString"""
        rsaPrivateKey = rsa.PrivateKey(self.RSA_N, self.RSA_E, self.RSA_D, self.RSA_P, self.RSA_Q)
        signature = rsa.sign(data, rsaPrivateKey, 'SHA-256')
        return byteStringToHexifiedBitString(signature)
