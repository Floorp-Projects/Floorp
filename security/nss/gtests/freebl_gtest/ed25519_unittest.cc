// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include <stdint.h>

#include "blapi.h"
#include "nss_scoped_ptrs.h"
#include "secerr.h"

namespace nss_test {

class EDDSATest : public ::testing::Test {
 protected:
  std::vector<uint8_t> hexStringToBytes(std::string s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < s.length(); i += 2) {
      bytes.push_back(std::stoul(s.substr(i, 2), nullptr, 16));
    }
    return bytes;
  }
  std::string bytesToHexString(std::vector<uint8_t> bytes) {
    std::stringstream s;
    for (auto b : bytes) {
      s << std::setfill('0') << std::setw(2) << std::uppercase << std::hex
        << static_cast<int>(b);
    }
    return s.str();
  }

  void TestEd25519_Sign(const std::string secret, const std::string p,
                        const std::string msg, const std::string signature) {
    std::vector<uint8_t> secret_bytes = hexStringToBytes(secret);
    ASSERT_GT(secret_bytes.size(), 0U);
    SECItem secret_value = {siBuffer, secret_bytes.data(),
                            static_cast<unsigned int>(secret_bytes.size())};

    std::vector<uint8_t> msg_bytes = hexStringToBytes(msg);
    const SECItem msg_value = {siBuffer, msg_bytes.data(),
                               static_cast<unsigned int>(msg_bytes.size())};

    std::vector<uint8_t> public_bytes = hexStringToBytes(p);
    const SECItem public_value = {
        siBuffer, public_bytes.data(),
        static_cast<unsigned int>(public_bytes.size())};

    ScopedSECItem signature_item(
        SECITEM_AllocItem(nullptr, nullptr, ED25519_SIGN_LEN));

    ECPrivateKey key;
    key.privateValue = secret_value;

    ECParams ecParams = {0};

    ScopedSECItem ecEncodedParams(SECITEM_AllocItem(nullptr, nullptr, 0U));
    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    ASSERT_TRUE(arena && ecEncodedParams);

    ecParams.name = ECCurve_Ed25519;
    key.ecParams = ecParams;

    SECStatus rv = ED_SignMessage(&key, signature_item.get(), &msg_value);
    ASSERT_EQ(SECSuccess, rv);

    ECPublicKey public_key;
    public_key.publicValue = public_value;
    public_key.ecParams = ecParams;

    rv = ED_VerifyMessage(&public_key, signature_item.get(), &msg_value);
    ASSERT_EQ(SECSuccess, rv);

    std::string signature_result = bytesToHexString(std::vector<uint8_t>(
        signature_item->data, signature_item->data + signature_item->len));
    EXPECT_EQ(signature_result, signature);
  }
};

TEST_F(EDDSATest, TestEd25519_Sign) {
  TestEd25519_Sign(
      "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
      "92A009A9F0D4CAB8720E820B5F642540A2B27B5416503F8FB3762223EBDB69DA085AC1E4"
      "3E15996E458F3613D0F11D8C387B2EAEB4302AEEB00D291612BB0C00");
}
TEST_F(EDDSATest, TestEd25519_Sign2) {
  TestEd25519_Sign(
      "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
      "E5564300C360AC729086E2CC806E828A84877F1EB8E5D974D873E065224901555FB88215"
      "90A33BACC61E39701CF9B46BD25BF5F0595BBE24655141438E7A100B");
}
TEST_F(EDDSATest, TestEd25519_Sign3) {
  TestEd25519_Sign(
      "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025",
      "af82",
      "6291D657DEEC24024827E69C3ABE01A30CE548A284743A445E3680D7DB5AC3AC18FF9B53"
      "8D16F290AE67F760984DC6594A7C15E9716ED28DC027BECEEA1EC40A");
}
TEST_F(EDDSATest, TestEd25519_Sign4) {
  TestEd25519_Sign(
      "f5e5767cf153319517630f226876b86c8160cc583bc013744c6bf255f5cc0ee5",
      "278117fc144c72340f67d0f2316e8386ceffbf2b2428c9c51fef7c597f1d426e",
      "08b8b2b733424243760fe426a4b54908632110a66c2f6591eabd3345e3e4eb98fa6e264b"
      "f09efe12ee50f8f54e9f77b1e355f6c50544e23fb1433ddf73be84d879de7c0046dc4996"
      "d9e773f4bc9efe5738829adb26c81b37c93a1b270b20329d658675fc6ea534e0810a4432"
      "826bf58c941efb65d57a338bbd2e26640f89ffbc1a858efcb8550ee3a5e1998bd177e93a"
      "7363c344fe6b199ee5d02e82d522c4feba15452f80288a821a579116ec6dad2b3b310da9"
      "03401aa62100ab5d1a36553e06203b33890cc9b832f79ef80560ccb9a39ce767967ed628"
      "c6ad573cb116dbefefd75499da96bd68a8a97b928a8bbc103b6621fcde2beca1231d206b"
      "e6cd9ec7aff6f6c94fcd7204ed3455c68c83f4a41da4af2b74ef5c53f1d8ac70bdcb7ed1"
      "85ce81bd84359d44254d95629e9855a94a7c1958d1f8ada5d0532ed8a5aa3fb2d17ba70e"
      "b6248e594e1a2297acbbb39d502f1a8c6eb6f1ce22b3de1a1f40cc24554119a831a9aad6"
      "079cad88425de6bde1a9187ebb6092cf67bf2b13fd65f27088d78b7e883c8759d2c4f5c6"
      "5adb7553878ad575f9fad878e80a0c9ba63bcbcc2732e69485bbc9c90bfbd62481d9089b"
      "eccf80cfe2df16a2cf65bd92dd597b0707e0917af48bbb75fed413d238f5555a7a569d80"
      "c3414a8d0859dc65a46128bab27af87a71314f318c782b23ebfe808b82b0ce26401d2e22"
      "f04d83d1255dc51addd3b75a2b1ae0784504df543af8969be3ea7082ff7fc9888c144da2"
      "af58429ec96031dbcad3dad9af0dcbaaaf268cb8fcffead94f3c7ca495e056a9b47acdb7"
      "51fb73e666c6c655ade8297297d07ad1ba5e43f1bca32301651339e22904cc8c42f58c30"
      "c04aafdb038dda0847dd988dcda6f3bfd15c4b4c4525004aa06eeff8ca61783aacec57fb"
      "3d1f92b0fe2fd1a85f6724517b65e614ad6808d6f6ee34dff7310fdc82aebfd904b01e1d"
      "c54b2927094b2db68d6f903b68401adebf5a7e08d78ff4ef5d63653a65040cf9bfd4aca7"
      "984a74d37145986780fc0b16ac451649de6188a7dbdf191f64b5fc5e2ab47b57f7f7276c"
      "d419c17a3ca8e1b939ae49e488acba6b965610b5480109c8b17b80e1b7b750dfc7598d5d"
      "5011fd2dcc5600a32ef5b52a1ecc820e308aa342721aac0943bf6686b64b2579376504cc"
      "c493d97e6aed3fb0f9cd71a43dd497f01f17c0e2cb3797aa2a2f256656168e6c496afc5f"
      "b93246f6b1116398a346f1a641f3b041e989f7914f90cc2c7fff357876e506b50d334ba7"
      "7c225bc307ba537152f3f1610e4eafe595f6d9d90d11faa933a15ef1369546868a7f3a45"
      "a96768d40fd9d03412c091c6315cf4fde7cb68606937380db2eaaa707b4c4185c32eddcd"
      "d306705e4dc1ffc872eeee475a64dfac86aba41c0618983f8741c5ef68d3a101e8a3b8ca"
      "c60c905c15fc910840b94c00a0b9d0",
      "0AAB4C900501B3E24D7CDF4663326A3A87DF5E4843B2CBDB67CBF6E460FEC350AA5371B1"
      "508F9F4528ECEA23C436D94B5E8FCD4F681E30A6AC00A9704A188A03");
}
TEST_F(EDDSATest, TestEd25519_Sign5) {
  TestEd25519_Sign(
      "833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
      "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf",
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a"
      "274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
      "DC2A4459E7369633A52B1BF277839A00201009A3EFBF3ECB69BEA2186C26B58909351FC9"
      "AC90B3ECFDFBC7C66431E0303DCA179C138AC17AD9BEF1177331A704");
}

}  // namespace nss_test
