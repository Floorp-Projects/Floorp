#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Reads a key specification from stdin or a file and outputs a
PKCS #8 file representing the (private) key. Also provides
methods for signing data and representing the key as a subject
public key info for use with pyasn1.

The key specification format is as follows:

<empty string>: a 2048-bit RSA key
alternate: a different 2048-bit RSA key
ev: a 2048-bit RSA key that, when combined with the right pycert
    specification, results in a certificate that is enabled for
    extended validation in debug Firefox (see ExtendedValidation.cpp).
evRSA2040: a 2040-bit RSA key that, when combined with the right pycert
           specification, results in a certificate that is enabled for
           extended validation in debug Firefox.
rsa2040: a 2040-bit RSA key

In the future it will be possible to specify other properties of the key
(type, strength, signature algorithm, etc.).
"""

from pyasn1.codec.der import encoder
from pyasn1.type import univ, namedtype
from pyasn1_modules import rfc2459
import base64
import binascii
import rsa
import sys

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


class RSAPrivateKey(univ.Sequence):
    """Helper type for encoding an RSA private key"""
    componentType = namedtype.NamedTypes(
        namedtype.NamedType('version', univ.Integer()),
        namedtype.NamedType('modulus', univ.Integer()),
        namedtype.NamedType('publicExponent', univ.Integer()),
        namedtype.NamedType('privateExponent', univ.Integer()),
        namedtype.NamedType('prime1', univ.Integer()),
        namedtype.NamedType('prime2', univ.Integer()),
        namedtype.NamedType('exponent1', univ.Integer()),
        namedtype.NamedType('exponent2', univ.Integer()),
        namedtype.NamedType('coefficient', univ.Integer()),
    )


class PrivateKeyInfo(univ.Sequence):
    """Helper type for encoding a PKCS #8 private key info"""
    componentType = namedtype.NamedTypes(
        namedtype.NamedType('version', univ.Integer()),
        namedtype.NamedType('privateKeyAlgorithm', rfc2459.AlgorithmIdentifier()),
        namedtype.NamedType('privateKey', univ.OctetString())
    )


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

    evRSA_N = long(
        '00b549895c9d00108d11a1f99f87a9e3d1a5db5dfaecf188da57bf641368'
        '8f2ce4722cff109038c17402c93a2a473dbd286aed3fdcd363cf5a291477'
        '01bdd818d7615bf9356bd5d3c8336aaa8c0971368a06c3cd4461b93e5142'
        '4e1744bb2eaad46aab38ce196821961f87714a1663693f09761cdf4d6ba1'
        '25eacec7be270d388f789f6cdf78ae3144ed28c45e79293863a7a22a4898'
        '0a36a40e72d579c9b925dff8c793362ffd6897a7c1754c5e97c967c3eadd'
        '1aae8aa2ccce348a0169b80e28a2d70c1a960c6f335f2da09b9b643f5abf'
        'ba49e8aaa981e960e27d87480bdd55dd9417fa18509fbb554ccf81a4397e'
        '8ba8128a34bdf27865c189e5734fb22905', 16)
    evRSA_E = 65537L
    evRSA_D = long(
        '00983d54f94d6f4c76eb23d6f93d78523530cf73b0d16254c6e781768d45'
        'f55681d1d02fb2bd2aac6abc1c389860935c52a0d8f41482010394778314'
        '1d864bff30803638a5c0152570ae9d18f3d8ca163efb475b0dddf32e7e16'
        'ec7565e6bb5e025c41c5c66e57a03cede554221f83045347a2c4c451c3dc'
        'e476b787ce0c057244be9e04ef13118dbbb3d5e0a6cc87029eafd4a69ed9'
        'b14759b15e39d8a9884e56f54d2f9ab013f0d15f318a9ab6b2f73d1ec3c9'
        'fe274ae89431a10640be7899b0011c5e5093a1834708689de100634dabde'
        '60fbd6aaefa3a33df34a1f36f60c043036b748d1c9ee98c4031a0afec60e'
        'fda0a990be524f5614eac4fdb34a52f951', 16)
    evRSA_P = long(
        '00eadc2cb33e5ff1ca376bbd95bd6a1777d2cf4fac47545e92d11a6209b9'
        'd5e4ded47834581c169b3c884742a09ea187505c1ca55414d8d25b497632'
        'd5ec2aaa05233430fad49892777a7d68e038f561a3b8969e60b0a263defb'
        'fda48a9b0ff39d95bc88b15267c8ade97b5107948e41e433249d87f7db10'
        '9d5d74584d86bcc1d7', 16)
    evRSA_Q = long(
        '00c59ae576a216470248d944a55b9e9bf93299da341ec56e558eba821abc'
        'e1bf57b79cf411d2904c774f9dba1f15185f607b0574a08205d6ec28b66a'
        '36d634232eaaf2fea37561abaf9d644b68db38c9964cb8c96ec0ac61eba6'
        '4d05b446542f423976f5acde4ecc95536d2df578954f93f0cfd9c58fb78b'
        'a2a76dd5ac284dc883', 16)
    evRSA_exp1 = long(
        '00c1d2ef3906331c52aca64811f9fe425beb2898322fb3db51032ce8d7e9'
        'fc32240be92019cf2480fcd5e329837127118b2a59a1bfe06c883e3a4447'
        'f3f031cd9aebd0b8d368fc79740d2cce8eadb324df7f091eafe1564361d5'
        '4920b01b0471230e5e47d93f8ed33963c517bc4fc78f6d8b1f9eba85bcce'
        'db7033026508db6285', 16)
    evRSA_exp2 = long(
        '008521b8db5694dfbe804a315f9efc9b65275c5490acf2a3456d65e6e610'
        'bf9f647fc67501d4f5772f232ac70ccdef9fc2a6dfa415c7c41b6afc7af9'
        'd07c3ca03f7ed93c09f0b99f2c304434322f1071709bbc1baa4c91575fa6'
        'a959e07d4996956d95e22b57938b6e47c8d51ffedfc9bf888ce0d1a3e42b'
        '65a89bed4b91d3e5f5', 16)
    evRSA_coef = long(
        '00dc497b06b920c8be0b0077b798e977eef744a90ec2c5d7e6cbb22448fa'
        'c72da81a33180e0d8a02e831460c7fc7fd3a612f7b9930b61b799f8e908e'
        '632e9ba0409b6aa70b03a3ba787426263b5bd5843df8476edb5d14f6a861'
        '3ebaf5b9cd5ca42f5fbd2802e08e4e49e5709f5151510caa5ab2c1c6eb3e'
        'fe9295d16e8c25c916', 16)

    evRSA2040_N = long(
        '00ca7020dc215f57914d343fae4a015111697af997a5ece91866499fc23f'
        '1b88a118cbd30b10d91c7b9a0d4ee8972fcae56caf57f25fc1275a2a4dbc'
        'b982428c32ef587bf2387410330a0ffb16b8029bd783969ef675f6de38c1'
        '8f67193cb6c072f8b23d0b3374112627a57b90055771d9e62603f53788d7'
        'f63afa724f5d108096df31f89f26b1eb5f7c4357980e008fcd55d827dd26'
        '2395ca2f526a07897cc40c593b38716ebc0caa596719c6f29ac9b73a7a94'
        '4748a3aa3e09e9eb4d461ea0027e540926614728b9d243975cf9a0541bef'
        'd25e76b51f951110b0e7644fc7e38441791b6d2227384cb8004e23342372'
        'b1cf5cc3e73e31b7bbefa160e6862ebb', 16)
    evRSA2040_E = 65537L
    evRSA2040_D = long(
        '00b2db74bce92362abf72955a638ae8720ba3033bb7f971caf39188d7542'
        'eaa1c1abb5d205b1e2111f4791c08911a2e141e8cfd7054702d23100b564'
        '2c06e1a31b118afd1f9a2f396cced425c501d91435ca8656766ced2b93bb'
        'b8669fce9bacd727d1dacb3dafabc3293e35389eef8ea0b58e1aeb1a20e6'
        'a61f9fcd453f7567fe31d123b616a26fef4df1d6c9f7490111d028eefd1d'
        '972045b1a242273dd7a67ebf111db2741a5a93c7b2289cc4a236f5a99a6e'
        'c7a8206fdae1c1d04bdbb1980d4a298c5a17dae4186474a5f7835d882bce'
        'f24aef4ed6f149f94d96c9f7d78e647fc778a9017ff208d3b4a1768b1821'
        '62102cdab032fabbab38d5200a324649', 16)
    evRSA2040_P = long(
         '0f3844d0d4d4d6a21acd76a6fc370b8550e1d7ec5a6234172e790f0029ae'
        '651f6d5c59330ab19802b9d7a207de7a1fb778e3774fdbdc411750633d8d'
        '1b3fe075006ffcfd1d10e763c7a9227d2d5f0c2dade1c9e659c350a159d3'
        '6bb986f12636d4f9942b288bc0fe21da8799477173144249ca2e389e6c5c'
        '25aa78c8cad7d4df', 16)
    evRSA2040_Q = long(
        '0d4d0bedd1962f07a1ead6b23a4ed67aeaf1270f052a6d29ba074945c636'
        '1a5c4f8f07bf859e067aed3f4e6e323ef2aa8a6acd340b0bdc7cfe4fd329'
        'e3c97f870c7f7735792c6aa9d0f7e7542a28ed6f01b0e55a2b8d9c24a65c'
        '6da314c95484f5c7c3954a81bb016b07ed17ee9b06039695bca059a79f8d'
        'c2423d328d5265a5', 16)
    evRSA2040_exp1 = long(
        '09f29a2ff05be8a96d614ba31b08935420a86c6bc42b99a6692ea0da5763'
        'f01e596959b7ddce73ef9c2e4f6e5b40710887500d44ba0c3cd3132cba27'
        '475f39c2df7552e2d123a2497a4f97064028769a48a3624657f72bf539f3'
        'd0de234feccd3be8a0aa90c6bf6e9b0bed43070a24d061ff3ed1751a3ef2'
        'ff7f6b90b9dbd5fb', 16)
    evRSA2040_exp2 = long(
        '01a659e170cac120a03be1cf8f9df1caa353b03593bd7476e5853bd874c2'
        '87388601c6c341ce9d1d284a5eef1a3a669d32b816a5eaecd8b7844fe070'
        '64b9bca0c2b318d540277b3f7f1510d386bb36e03b04771e5d229e88893e'
        '13b753bfb94518bb638e2404bd6e6a993c1668d93fc0b82ff08aaf34347d'
        '3fe8397108c87ca5', 16)
    evRSA2040_coef = long(
        '040257c0d4a21c0b9843297c65652db66304fb263773d728b6abfa06d37a'
        'c0ca62c628023e09e37dc0a901e4ce1224180e2582a3aa4b6a1a7b98e2bd'
        '70077aec14ac8ab66a755c71e0fc102471f9bbc1b46a95aa0b645f2c38e7'
        '6450289619ea3f5e8ae61037bffcf8249f22aa4e76e2a01909f3feb290ce'
        '93edf57b10ebe796', 16)

    rsa2040_N = long(
      '00bac0652fdfbc0055882ffbaeaceec88fa2d083c297dd5d40664dd3d90f'
      '52f9aa02bd8a50fba16e0fd991878ef475f9b350d9f8e3eb2abd717ce327'
      'b09788531f13df8e3e4e3b9d616bb8a41e5306eed2472163161051180127'
      '6a4eb66f07331b5cbc8bcae7016a8f9b3d4f2ac4553c624cf5263bcb348e'
      '8840de6612870960a792191b138fb217f765cec7bff8e94f16b39419bf75'
      '04c59a7e4f79bd6d173e9c7bf3d9d2a4e73cc180b0590a73d584fb7fc9b5'
      '4fa544607e53fc685c7a55fd44a81d4142b6af51ea6fa6cea52965a2e8c5'
      'd84f3ca024d6fbb9b005b9651ce5d9f2ecf40ed404981a9ffc02636e311b'
      '095c6332a0c87dc39271b5551481774b', 16)
    rsa2040_E = 65537L
    rsa2040_D = long(
      '603db267df97555cbed86b8df355034af28f1eb7f3e7829d239bcc273a7c'
      '7a69a10be8f21f1b6c4b02c6bae3731c3158b5bbff4605f57ab7b7b2a0cb'
      'a2ec005a2db5b1ea6e0aceea5bc745dcd2d0e9d6b80d7eb0ea2bc08127bc'
      'e35fa50c42cc411871ba591e23ba6a38484a33eff1347f907ee9a5a92a23'
      '11bb0b435510020f78e3bb00099db4d1182928096505fcba84f3ca1238fd'
      '1eba5eea1f391bbbcc5424b168063fc17e1ca6e1912ccba44f9d0292308a'
      '1fedb80612529b39f59d0a3f8180b5ba201132197f93a5815ded938df8e7'
      'd93c9b15766588f339bb59100afda494a7e452d7dd4c9a19ce2ec3a33a18'
      'b20f0b4dade172bee19f26f0dcbe41', 16)
    rsa2040_P = long(
      '0ec3869cb92d406caddf7a319ab29448bc505a05913707873361fc5b986a'
      '499fb65eeb815a7e37687d19f128087289d9bb8818e7bcca502c4900ad9a'
      'ece1179be12ff3e467d606fc820ea8f07ac9ebffe2236e38168412028822'
      '3e42dbe68dfd972a85a6447e51695f234da7911c67c9ab9531f33df3b994'
      '32d4ee88c9a4efbb', 16)
    rsa2040_Q = long(
      '0ca63934549e85feac8e0f5604303fd1849fe88af4b7f7e1213283bbc7a2'
      'c2a509f9273c428c68de3db93e6145f1b400bd6d4a262614e9043ad362d4'
      'eba4a6b995399c8934a399912199e841d8e8dbff0489f69e663796730b29'
      '80530b31cb70695a21625ea2adccc09d930516fa872211a91e22dd89fd9e'
      'b7da8574b72235b1', 16)
    rsa2040_exp1 = long(
      '0d7d3a75e17f65f8a658a485c4095c10a4f66979e2b73bca9cf8ef21253e'
      '1facac6d4791f58392ce8656f88f1240cc90c29653e3100c6d7a38ed44b1'
      '63b339e5f3b6e38912126c69b3ceff2e5192426d9649b6ffca1abb75d2ba'
      '2ed6d9a26aa383c5973d56216ff2edb90ccf887742a0f183ac92c94cf187'
      '657645c7772d9ad7', 16)
    rsa2040_exp2 = long(
      '03f550194c117f24bea285b209058032f42985ff55acebe88b16df9a3752'
      '7b4e61dc91a68dbc9a645134528ce5f248bda2893c96cb7be79ee73996c7'
      'c22577f6c2f790406f3472adb3b211b7e94494f32c5c6fcc0978839fe472'
      '4c31b06318a2489567b4fca0337acb1b841227aaa5f6c74800a2306929f0'
      '2ce038bad943df41', 16)
    rsa2040_coef = long(
      '080a7dbfa8c2584814c71664c56eb62ce4caf16afe88d4499159d674774a'
      '3a3ecddf1256c02fc91525c527692422d0aba94e5c41ee12dc71bb66f867'
      '9fa17e096f28080851ba046eb31885c1414e8985ade599d907af17453d1c'
      'caea2c0d06443f8367a6be154b125e390ee0d90f746f08801dd3f5367f59'
      'fba2e5a67c05f375', 16)

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
        elif specification == 'ev':
            self.RSA_N = self.evRSA_N
            self.RSA_E = self.evRSA_E
            self.RSA_D = self.evRSA_D
            self.RSA_P = self.evRSA_P
            self.RSA_Q = self.evRSA_Q
            self.RSA_exp1 = self.evRSA_exp1
            self.RSA_exp2 = self.evRSA_exp2
            self.RSA_coef = self.evRSA_coef
        elif specification == 'evRSA2040':
            self.RSA_N = self.evRSA2040_N
            self.RSA_E = self.evRSA2040_E
            self.RSA_D = self.evRSA2040_D
            self.RSA_P = self.evRSA2040_P
            self.RSA_Q = self.evRSA2040_Q
            self.RSA_exp1 = self.evRSA2040_exp1
            self.RSA_exp2 = self.evRSA2040_exp2
            self.RSA_coef = self.evRSA2040_coef
        elif specification == 'rsa2040':
            self.RSA_N = self.rsa2040_N
            self.RSA_E = self.rsa2040_E
            self.RSA_D = self.rsa2040_D
            self.RSA_P = self.rsa2040_P
            self.RSA_Q = self.rsa2040_Q
            self.RSA_exp1 = self.rsa2040_exp1
            self.RSA_exp2 = self.rsa2040_exp2
            self.RSA_coef = self.rsa2040_coef
        else:
            raise UnknownKeySpecificationError(specification)

    def toDER(self):
        privateKeyInfo = PrivateKeyInfo()
        privateKeyInfo.setComponentByName('version', 0)
        algorithmIdentifier = rfc2459.AlgorithmIdentifier()
        algorithmIdentifier.setComponentByName('algorithm', rfc2459.rsaEncryption)
        algorithmIdentifier.setComponentByName('parameters', univ.Null())
        privateKeyInfo.setComponentByName('privateKeyAlgorithm', algorithmIdentifier)
        rsaPrivateKey = RSAPrivateKey()
        rsaPrivateKey.setComponentByName('version', 0)
        rsaPrivateKey.setComponentByName('modulus', self.RSA_N)
        rsaPrivateKey.setComponentByName('publicExponent', self.RSA_E)
        rsaPrivateKey.setComponentByName('privateExponent', self.RSA_D)
        rsaPrivateKey.setComponentByName('prime1', self.RSA_P)
        rsaPrivateKey.setComponentByName('prime2', self.RSA_Q)
        rsaPrivateKey.setComponentByName('exponent1', self.RSA_exp1)
        rsaPrivateKey.setComponentByName('exponent2', self.RSA_exp2)
        rsaPrivateKey.setComponentByName('coefficient', self.RSA_coef)
        rsaPrivateKeyEncoded = encoder.encode(rsaPrivateKey)
        privateKeyInfo.setComponentByName('privateKey', univ.OctetString(rsaPrivateKeyEncoded))
        return encoder.encode(privateKeyInfo)

    def toPEM(self):
        output = '-----BEGIN PRIVATE KEY-----'
        der = self.toDER()
        b64 = base64.b64encode(der)
        while b64:
            output += '\n' + b64[:64]
            b64 = b64[64:]
        output += '\n-----END PRIVATE KEY-----'
        return output

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


# The build harness will call this function with an output file-like
# object and a path to a file containing a specification. This will
# read the specification and output the key as ASCII-encoded PKCS #8.
def main(output, inputPath):
    with open(inputPath) as configStream:
        output.write(RSAKey(configStream.read().strip()).toPEM())

# When run as a standalone program, this will read a specification from
# stdin and output the certificate as PEM to stdout.
if __name__ == '__main__':
    print RSAKey(sys.stdin.read()).toPEM()
