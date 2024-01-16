// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

/* The tests are taken from the RFC 9474. */

#include "gtest/gtest.h"

#include <stdint.h>

#include "blapi.h"
#include "nss_scoped_ptrs.h"
#include "secerr.h"

namespace nss_test {

class RSABlindTest : public ::testing::Test {
 protected:
  std::vector<uint8_t> hexStringToBytes(std::string s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < s.length(); i += 2) {
      bytes.push_back(std::stoul(s.substr(i, 2), nullptr, 16));
    }
    return bytes;
  }

  void TestRSABlind(HASH_HashType hashAlg, const size_t nLen,
                    std::string modulus_str, std::string e_str,
                    std::string d_str, std::string m_str, std::string salt_str,
                    std::string random_str, PRBool isDeterministic,
                    std::string sign_expected_str) {
    std::vector<uint8_t> m = hexStringToBytes(m_str);
    std::vector<uint8_t> salt = hexStringToBytes(salt_str);
    std::vector<uint8_t> random = hexStringToBytes(random_str);
    std::vector<uint8_t> signature_expected =
        hexStringToBytes(sign_expected_str);

    std::vector<uint8_t> modulus_v = hexStringToBytes(modulus_str);
    std::vector<uint8_t> e_v = hexStringToBytes(e_str);
    std::vector<uint8_t> d_v = hexStringToBytes(d_str);

    SECItem empty = {siBuffer, nullptr, 0};
    SECItem modulus = {siBuffer, modulus_v.data(),
                       (unsigned int)modulus_v.size()};
    SECItem e = {siBuffer, e_v.data(), (unsigned int)e_v.size()};
    SECItem d = {siBuffer, d_v.data(), (unsigned int)d_v.size()};

    RSAPrivateKey key = {NULL,  empty, modulus, e,     d,
                         empty, empty, empty,   empty, empty};

    RSAPublicKey pkS = {NULL, key.modulus, key.publicExponent};

    SECStatus rv = SECFailure;

    size_t preparedMessageLen = m.size();
    if (!isDeterministic) {
      /* + 32 bytes of randomness. */
      preparedMessageLen += 32;
    }

    std::vector<PRUint8> preparedMessage(nLen);
    std::vector<PRUint8> blindedMsg(nLen);
    std::vector<PRUint8> blindedSig(nLen);
    std::vector<PRUint8> inv(nLen);
    std::vector<PRUint8> signature(nLen);
    PORT_Memset(preparedMessage.data(), 0, nLen);
    PORT_Memset(blindedMsg.data(), 0, nLen);
    PORT_Memset(blindedSig.data(), 0, nLen);
    PORT_Memset(inv.data(), 0, nLen);
    PORT_Memset(signature.data(), 0, nLen);

    rv = RSABlinding_Prepare(preparedMessage.data(), preparedMessageLen,
                             m.data(), m.size(), isDeterministic);

    EXPECT_EQ(SECSuccess, rv);
    rv = RSABlinding_Blind(hashAlg, blindedMsg.data(), nLen, inv.data(), nLen,
                           preparedMessage.data(), preparedMessageLen,
                           salt.data(), salt.size(), &pkS, random.data(),
                           random.size());

    EXPECT_EQ(SECSuccess, rv);
    rv = RSABlinding_BlindSign(blindedSig.data(), nLen, blindedMsg.data(), nLen,
                               &key, &pkS);
    EXPECT_EQ(SECSuccess, rv);
    rv = RSABlinding_Finalize(hashAlg, signature.data(), preparedMessage.data(),
                              preparedMessageLen, blindedSig.data(), nLen,
                              inv.data(), nLen, &pkS, salt.size());

    EXPECT_EQ(0, memcmp(signature.data(), signature_expected.data(), nLen));
    EXPECT_EQ(rv, SECSuccess);
  }
};

TEST_F(RSABlindTest, TestRSABlind) {
  TestRSABlind(
      HASH_AlgSHA384, 512,
      "aec4d69addc70b990ea66a5e70603b6fee27aafebd08f2d94cbe1250c556e047a928d635"
      "c3f45ee9b66d1bc628a03bac9b7c3f416fe20dabea8f3d7b4bbf7f963be335d2328d67e6"
      "c13ee4a8f955e05a3283720d3e1f139c38e43e0338ad058a9495c53377fc35be64d208f8"
      "9b4aa721bf7f7d3fef837be2a80e0f8adf0bcd1eec5bb040443a2b2792fdca522a7472ae"
      "d74f31a1ebe1eebc1f408660a0543dfe2a850f106a617ec6685573702eaaa21a5640a5dc"
      "af9b74e397fa3af18a2f1b7c03ba91a6336158de420d63188ee143866ee415735d155b7c"
      "2d854d795b7bc236cffd71542df34234221a0413e142d8c61355cc44d45bda9420497455"
      "7ac2704cd8b593f035a5724b1adf442e78c542cd4414fce6f1298182fb6d8e53cef1adfd"
      "2e90e1e4deec52999bdc6c29144e8d52a125232c8c6d75c706ea3cc06841c7bda33568c6"
      "3a6c03817f722b50fcf898237d788a4400869e44d90a3020923dc646388abcc914315215"
      "fcd1bae11b1c751fd52443aac8f601087d8d42737c18a3fa11ecd4131ecae017ae0a14ac"
      "fc4ef85b83c19fed33cfd1cd629da2c4c09e222b398e18d822f77bb378dea3cb360b605e"
      "5aa58b20edc29d000a66bd177c682a17e7eb12a63ef7c2e4183e0d898f3d6bf567ba8ae8"
      "4f84f1d23bf8b8e261c3729e2fa6d07b832e07cddd1d14f55325c6f924267957121902dc"
      "19b3b32948bdead5",
      "010001",
      "0d43242aefe1fb2c13fbc66e20b678c4336d20b1808c558b6e62ad16a287077180b177e1"
      "f01b12f9c6cd6c52630257ccef26a45135a990928773f3bd2fc01a313f1dac97a51cec71"
      "cb1fd7efc7adffdeb05f1fb04812c924ed7f4a8269925dad88bd7dcfbc4ef01020ebfc60"
      "cb3e04c54f981fdbd273e69a8a58b8ceb7c2d83fbcbd6f784d052201b88a9848186f2a45"
      "c0d2826870733e6fd9aa46983e0a6e82e35ca20a439c5ee7b502a9062e1066493bdadf8b"
      "49eb30d9558ed85abc7afb29b3c9bc644199654a4676681af4babcea4e6f71fe4565c9c1"
      "b85d9985b84ec1abf1a820a9bbebee0df1398aae2c85ab580a9f13e7743afd3108eb3210"
      "0b870648fa6bc17e8abac4d3c99246b1f0ea9f7f93a5dd5458c56d9f3f81ff2216b3c368"
      "0a13591673c43194d8e6fc93fc1e37ce2986bd628ac48088bc723d8fbe293861ca7a9f4a"
      "73e9fa63b1b6d0074f5dea2a624c5249ff3ad811b6255b299d6bc5451ba7477f19c5a0db"
      "690c3e6476398b1483d10314afd38bbaf6e2fbdbcd62c3ca9797a420ca6034ec0a83360a"
      "3ee2adf4b9d4ba29731d131b099a38d6a23cc463db754603211260e99d19affc902c915d"
      "7854554aabf608e3ac52c19b8aa26ae042249b17b2d29669b5c859103ee53ef9bdc73ba3"
      "c6b537d5c34b6d8f034671d7f3a8a6966cc4543df223565343154140fd7391c7e7be03e2"
      "41f4ecfeb877a051",
      "8f3dc6fb8c4a02f4d6352edf0907822c1210a9b32f9bdda4c45a698c80023aa6b59f8cfe"
      "c5fdbb36331372ebefedae7d",
      "051722b35f458781397c3a671a7d3bd3096503940e4c4f1aaa269d60300ce449555cd734"
      "0100df9d46944c5356825abf",
      "44f6af51f31e03943acf6fb47e805ce4794cb0861772d78890952d20f7aa76a2b841f18d"
      "290f6e02beda82f7d2a560ffd7af727019269699e67dbf8e7f60946515b253b9cda85706"
      "984ffb3176633e5135e73ca0bf8371df50a170286fb56399a0fd093d1a16b62ea5a60096"
      "0016e14f0079e7aa5824676adddea4ebaca2ec0473b462b8a50d57c962c1fcd68949f46f"
      "62beb9867f04db169508f0a3c8df0f67149b1425a0e1fc0321f0ab55b9208d515cfa8be6"
      "d82e7273f7c59b861c24b82dd379809fc0a21783ecc247d2e431311658359e7d18095327"
      "26536b89ccf684269eff88a9a33898091d28d6ffae70185d6cc8699c177dff5db4849e74"
      "b259405675b01c53eecc5ec03819ce000cf79f3da883653b85b3822e27d130791d67e339"
      "554d75393b2c210bf6f684b7c0f4a953187959563269d6ece8fa9a28b786b095ef81564c"
      "e02cfb68ec801258704b9311f6ef5aaf7cdac4266931e462364c27b4468689e9906aabe6"
      "669aebdc67510c7bc5016083b862039aacbee7ca15ae62b6b35287538adab56d2c9220bf"
      "b14e91e6ea4f42a159aeb3dbaffbea17b012594ed8f939411ea1e9177ec9a4cb3168463b"
      "a603340b2858d76bf8f9ae6197e2cdf0dd5636b32ea383ed377bd7f655ac8078a5bc49de"
      "a8cf27b2dcc22d81d734ea8d5c1643b3082fd1627933305fe962f326e614a3f3a74dac61"
      "ac09439a3e05f255",
      PR_TRUE,
      "6fef8bf9bc182cd8cf7ce45c7dcf0e6f3e518ae48f06f3c670c649ac737a8b8119a34d51"
      "641785be151a697ed7825fdfece82865123445eab03eb4bb91cecf4d6951738495f84811"
      "51b62de869658573df4e50a95c17c31b52e154ae26a04067d5ecdc1592c287550bb982a5"
      "bb9c30fd53a768cee6baabb3d483e9f1e2da954c7f4cf492fe3944d2fe456c1ecaf08403"
      "69e33fb4010e6b44bb1d721840513524d8e9a3519f40d1b81ae34fb7a31ee6b7ed641cb1"
      "6c2ac999004c2191de0201457523f5a4700dd649267d9286f5c1d193f1454c9f868a5781"
      "6bf5ff76c838a2eeb616a3fc9976f65d4371deecfbab29362caebdff69c635fe5a2113da"
      "4d4d8c24f0b16a0584fa05e80e607c5d9a2f765f1f069f8d4da21f27c2a3b5c984b4ab24"
      "899bef46c6d9323df4862fe51ce300fca40fb539c3bb7fe2dcc9409e425f2d3b95e70e9c"
      "49c5feb6ecc9d43442c33d50003ee936845892fb8be475647da9a080f5bc7f8a716590b3"
      "745c2209fe05b17992830ce15f32c7b22cde755c8a2fe50bd814a0434130b807dc1b7218"
      "d4e85342d70695a5d7f29306f25623ad1e8aa08ef71b54b8ee447b5f64e73d09bdd6c3b7"
      "ca224058d7c67cc7551e9241688ada12d859cb7646fbd3ed8b34312f3b49d69802f0eaa1"
      "1bc4211c2f7a29cd5c01ed01a39001c5856fab36228f5ee2f2e1110811872fe7c865c42e"
      "d59029c706195d52"

  );
}

TEST_F(RSABlindTest, TestRSABlindEmptySalt) {
  TestRSABlind(
      HASH_AlgSHA384, 512,
      "aec4d69addc70b990ea66a5e70603b6fee27aafebd08f2d94cbe1250c556e047a928d635"
      "c3f45ee9b66d1bc628a03bac9b7c3f416fe20dabea8f3d7b4bbf7f963be335d2328d67e6"
      "c13ee4a8f955e05a3283720d3e1f139c38e43e0338ad058a9495c53377fc35be64d208f8"
      "9b4aa721bf7f7d3fef837be2a80e0f8adf0bcd1eec5bb040443a2b2792fdca522a7472ae"
      "d74f31a1ebe1eebc1f408660a0543dfe2a850f106a617ec6685573702eaaa21a5640a5dc"
      "af9b74e397fa3af18a2f1b7c03ba91a6336158de420d63188ee143866ee415735d155b7c"
      "2d854d795b7bc236cffd71542df34234221a0413e142d8c61355cc44d45bda9420497455"
      "7ac2704cd8b593f035a5724b1adf442e78c542cd4414fce6f1298182fb6d8e53cef1adfd"
      "2e90e1e4deec52999bdc6c29144e8d52a125232c8c6d75c706ea3cc06841c7bda33568c6"
      "3a6c03817f722b50fcf898237d788a4400869e44d90a3020923dc646388abcc914315215"
      "fcd1bae11b1c751fd52443aac8f601087d8d42737c18a3fa11ecd4131ecae017ae0a14ac"
      "fc4ef85b83c19fed33cfd1cd629da2c4c09e222b398e18d822f77bb378dea3cb360b605e"
      "5aa58b20edc29d000a66bd177c682a17e7eb12a63ef7c2e4183e0d898f3d6bf567ba8ae8"
      "4f84f1d23bf8b8e261c3729e2fa6d07b832e07cddd1d14f55325c6f924267957121902dc"
      "19b3b32948bdead5",
      "010001",
      "0d43242aefe1fb2c13fbc66e20b678c4336d20b1808c558b6e62ad16a287077180b177e1"
      "f01b12f9c6cd6c52630257ccef26a45135a990928773f3bd2fc01a313f1dac97a51cec71"
      "cb1fd7efc7adffdeb05f1fb04812c924ed7f4a8269925dad88bd7dcfbc4ef01020ebfc60"
      "cb3e04c54f981fdbd273e69a8a58b8ceb7c2d83fbcbd6f784d052201b88a9848186f2a45"
      "c0d2826870733e6fd9aa46983e0a6e82e35ca20a439c5ee7b502a9062e1066493bdadf8b"
      "49eb30d9558ed85abc7afb29b3c9bc644199654a4676681af4babcea4e6f71fe4565c9c1"
      "b85d9985b84ec1abf1a820a9bbebee0df1398aae2c85ab580a9f13e7743afd3108eb3210"
      "0b870648fa6bc17e8abac4d3c99246b1f0ea9f7f93a5dd5458c56d9f3f81ff2216b3c368"
      "0a13591673c43194d8e6fc93fc1e37ce2986bd628ac48088bc723d8fbe293861ca7a9f4a"
      "73e9fa63b1b6d0074f5dea2a624c5249ff3ad811b6255b299d6bc5451ba7477f19c5a0db"
      "690c3e6476398b1483d10314afd38bbaf6e2fbdbcd62c3ca9797a420ca6034ec0a83360a"
      "3ee2adf4b9d4ba29731d131b099a38d6a23cc463db754603211260e99d19affc902c915d"
      "7854554aabf608e3ac52c19b8aa26ae042249b17b2d29669b5c859103ee53ef9bdc73ba3"
      "c6b537d5c34b6d8f034671d7f3a8a6966cc4543df223565343154140fd7391c7e7be03e2"
      "41f4ecfeb877a051",
      "8f3dc6fb8c4a02f4d6352edf0907822c1210a9b32f9bdda4c45a698c80023aa6b59f8cfe"
      "c5fdbb36331372ebefedae7d",
      "",
      "44f6af51f31e03943acf6fb47e805ce4794cb0861772d78890952d20f7aa76a2b841f18d"
      "290f6e02beda82f7d2a560ffd7af727019269699e67dbf8e7f60946515b253b9cda85706"
      "984ffb3176633e5135e73ca0bf8371df50a170286fb56399a0fd093d1a16b62ea5a60096"
      "0016e14f0079e7aa5824676adddea4ebaca2ec0473b462b8a50d57c962c1fcd68949f46f"
      "62beb9867f04db169508f0a3c8df0f67149b1425a0e1fc0321f0ab55b9208d515cfa8be6"
      "d82e7273f7c59b861c24b82dd379809fc0a21783ecc247d2e431311658359e7d18095327"
      "26536b89ccf684269eff88a9a33898091d28d6ffae70185d6cc8699c177dff5db4849e74"
      "b259405675b01c53eecc5ec03819ce000cf79f3da883653b85b3822e27d130791d67e339"
      "554d75393b2c210bf6f684b7c0f4a953187959563269d6ece8fa9a28b786b095ef81564c"
      "e02cfb68ec801258704b9311f6ef5aaf7cdac4266931e462364c27b4468689e9906aabe6"
      "669aebdc67510c7bc5016083b862039aacbee7ca15ae62b6b35287538adab56d2c9220bf"
      "b14e91e6ea4f42a159aeb3dbaffbea17b012594ed8f939411ea1e9177ec9a4cb3168463b"
      "a603340b2858d76bf8f9ae6197e2cdf0dd5636b32ea383ed377bd7f655ac8078a5bc49de"
      "a8cf27b2dcc22d81d734ea8d5c1643b3082fd1627933305fe962f326e614a3f3a74dac61"
      "ac09439a3e05f255",
      PR_TRUE,
      "4454b6983ff01cb28545329f394936efa42ed231e15efbc025fdaca00277acf0c8e00e3d"
      "8b0ecebd35b057b8ebfc14e1a7097368a4abd20b555894ccef3d1b9528c6bcbda6b95376"
      "bef230d0f1feff0c1064c62c60a7ae7431d1fdfa43a81eed9235e363e1ffa0b2797aba6a"
      "ad6082fcd285e14fc8b71de6b9c87cb4059c7dc1e96ae1e63795a1e9af86b9073d1d848a"
      "ef3eca8a03421bcd116572456b53bcfd4dabb0a9691f1fabda3ed0ce357aee2cfee5b1a0"
      "eb226f69716d4e011d96eede5e38a9acb531a64336a0d5b0bae3ab085b658692579a3767"
      "40ff6ce69e89b06f360520b864e33d82d029c808248a19e18e31f0ecd16fac5cd4870f8d"
      "3ebc1c32c718124152dc905672ab0b7af48bf7d1ac1ff7b9c742549c91275ab105458ae3"
      "7621757add83482bbcf779e777bbd61126e93686635d4766aedf5103cf7978f3856ccac9"
      "e28d21a850dbb03c811128616d315d717be1c2b6254f8509acae862042c034530329ce15"
      "ca2e2f6b1f5fd59272746e3918c748c0eb810bf76884fa10fcf749326bbfaa5ba285a018"
      "6a22e4f628dbf178d3bb5dc7e165ca73f6a55ecc14c4f5a26c4693ce5da032264cbec319"
      "b12ddb9787d0efa4fcf1e5ccee35ad85ecd453182df9ed735893f830b570faae8be0f6fe"
      "2e571a4e0d927cba4debd368d3b4fca33ec6251897a137cf75474a32ac8256df5e5ffa51"
      "8b88b43fb6f63a24"

  );
}

}  // namespace nss_test
