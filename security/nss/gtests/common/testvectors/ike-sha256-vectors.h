/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling python3 genTestVectors.py */

#ifndef ike_sha256_vectors_h__
#define ike_sha256_vectors_h__

#include "testvectors_base/test-structs.h"

const IkeTestVector kIkeSha256ProofVectors[] = {
    // these vectors are from this NIST samples
    {1, IkeTestType::ikeGxy,
     "a1ff3dc6cf9b4c04709943cb4ca1f1789bcf360b03f1d027de3ae8ee039e9155", "", "",
     "750c5c94b9c2ec20b68033e024dadf0fa87e8b48c6561b21c72478451a06583d",
     "b1dee62505b47b223bae14ce7a5b757402ad1587511618d09f94950d47f1d8d4ce86aca12"
     "d78db9854d86019ad735757ae79d8932ac0c7db842c85060150ca875ea5d47e3cfcb2a059"
     "22ebb7959d49b9797a2289676ee79a1d9a18b790f87e4771ddaf4be3376057a553162f68f"
     "e429aca73b07234543801ba2122b1bde82251770d05df813cf556a11ca4dc43ffcb85a97d"
     "bed16e2fda6985e07e31be6364899e63c507c7c616e5eb7765a53560f76772de43918ba07"
     "badfe85244dcdcd917cb065afb60e3b7e68b54dd94bfc7c31c8b752892781ed3cc4b7f28f"
     "bc0ab9af908f5ae1f09f893f80100a7b3135993161b51fbba3bbb24b9f88c6147de82cd6f"
     "0",
     "f0acfef2ad1f7add0eaafda78c1cf1097d9fc91cb04a7c145069ac426fd164cbe661b1dd2"
     "df0fb84e19512181f0d8ea50b7860845f332757a8e56d2a3b7be436b5718a2d49baa996a4"
     "616684a208c2d611cd65e605dca6e3d3f116859b4410fe13679696bb2e23c08a40c7e1316"
     "d54b4c9c0286701c221151b3642cb4112ca1a53e0e597a7e29c634caed86ca3c31973d37b"
     "4c346134fd6784cd99913feedf3d29d89a0a02a5a750f02f5738109dcc670bb27701fb59f"
     "78e83b76860c3fec079a1fc8c937ddb58ae7500422b7e49ce63759c65b6bc439381d56bcc"
     "159edede894b073841036ebfa050a5b3e7c876a3f18def26b1768a263ac66c9d83b680eb5"
     "e",
     "", 0, 0, true},
    {2, IkeTestType::ikeV1,
     "750c5c94b9c2ec20b68033e024dadf0fa87e8b48c6561b21c72478451a06583d",
     "a1ff3dc6cf9b4c04709943cb4ca1f1789bcf360b03f1d027de3ae8ee039e9155", "",
     "a4f7ca7de913814813e3312099e7c943bd293483f387532330237f1b20957310",
     "6c6beb72631ddc3d", "b84e24b22cffbd14", "", 0, 0, true},
    {3, IkeTestType::ikeV1,
     "750c5c94b9c2ec20b68033e024dadf0fa87e8b48c6561b21c72478451a06583d",
     "a1ff3dc6cf9b4c04709943cb4ca1f1789bcf360b03f1d027de3ae8ee039e9155",
     "a4f7ca7de913814813e3312099e7c943bd293483f387532330237f1b20957310",
     "1d4b705746c43b0a6fcbb8db33983c0f24ff6f8b6543e3779fed227c6067f004",
     "6c6beb72631ddc3d", "b84e24b22cffbd14", "", 1, 0, true},
    {4, IkeTestType::ikeV1,
     "750c5c94b9c2ec20b68033e024dadf0fa87e8b48c6561b21c72478451a06583d",
     "a1ff3dc6cf9b4c04709943cb4ca1f1789bcf360b03f1d027de3ae8ee039e9155",
     "1d4b705746c43b0a6fcbb8db33983c0f24ff6f8b6543e3779fed227c6067f004",
     "03e6f16cd9ce9f64b5cdc5b34cca7163483ba5389a30afebef3d14640b0a815e",
     "6c6beb72631ddc3d", "b84e24b22cffbd14", "", 2, 0, true},
    {5, IkeTestType::ikeV1Psk, "a0", "", "",
     "558a99b299773d267cf7c8ef073bf3b7af362c206c75a538403c5ef884d4cace",
     "ead9ced494868f41", "f1aff4f425a94f18", "", 0, 0, true},
    {6, IkeTestType::ikeGxy,
     "0f4d257d7a58fc4545c7d7a88119eee5d5c9690c5b4c989171d3abbfd99d1d29", "", "",
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe",
     "3f302be1abcb28e1", "8c332ee006064c9b", "", 0, 0, true},
    {7, IkeTestType::ikeV2Rekey,
     "0b137d669b0947d7d026d593f0305ad401ff0c471357d695778a9c7f4b4869ec",
     "25f3b12d6f282739256e39bf54eda53b60ffcf379bb7bcc90c27b4c4c578616c", "",
     "2d63f6debc92048b4fef3889c4c99ca67d6496e0fac14a2bca9a2d6566ff2398",
     "3f302be1abcb28e1", "8c332ee006064c9b", "", 0, 0, true},
    {8, IkeTestType::ikePlus,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "0b137d669b0947d7d026d593f0305ad401ff0c471357d695778a9c7f4b4869ece98aca531"
     "188d16041b3bb936d2dbb3b4993a6e768a809160de45d0283f273a6cdf6854379e31be72b"
     "8d3d1fa990cf9c5b015ca9f918a7df6253c958114a09d4e1c19bdcd4db14b29d98db1a74a"
     "d405c588662c14a04d0d36aa4ab55e90f8986d12d4aad",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "3f302be1abcb28e1"   // Ni
     "8c332ee006064c9b"   // Nr
     "40dac39e1e1a8640"   // SPIi
     "8619a1cf9a6e4c07",  // SPIr
     0, 132, true},
    {9, IkeTestType::ikePlus,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "0b137d669b0947d7d026d593f0305ad401ff0c471357d695778a9c7f4b4869ec", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "3f302be1abcb28e1"   // Ni
     "8c332ee006064c9b"   // Nr
     "40dac39e1e1a8640"   // SPIi
     "8619a1cf9a6e4c07",  // SPIr
     0, 32, true},
    {10, IkeTestType::ikePlus,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "0b137d669b0947d7d026d593f0305a", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "3f302be1abcb28e1"   // Ni
     "8c332ee006064c9b"   // Nr
     "40dac39e1e1a8640"   // SPIi
     "8619a1cf9a6e4c07",  // SPIr
     0, 15, true},
    // these vectors are self-generated
    {11, IkeTestType::ikeV1AppB,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "b10fff32cbeaa1e7afe6ab0b191e0bd63cd524849a4b56019146d232a24cf9af6b89494d2"
     "a360b06825db8bb0324c15cecf47fc0bc99e39bf1171a7f4bf1733dc49ef64c642e73b054"
     "b2e82456e34fa3c822da475e27e403b3da3929da50e6aa9e7f9252c68fa069b4b0edd374e"
     "80d35378c4f5e8ec285a1b169c92bbb5353d05ba94165",
     "", "", "", 0, 132, true},
    {12, IkeTestType::ikeV1AppB,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "", 0, 32, true},
    {13, IkeTestType::ikeV1AppB,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "5f00d1bd2c58ec224b1e6b71fa0f19", "", "", "", 0, 15, true},
    {14, IkeTestType::ikeV1AppBQuick,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "b10fff32cbeaa1e7afe6ab0b191e0bd63cd524849a4b56019146d232a24cf9af59f18ed9a"
     "abbb2dbbafecf48d72a34a8f72fab2ff4f37e5c917288a78ce00933612e9531a7469995c7"
     "f7cc33c7627cac3efbc819330c4fe3bfa3788799630f37bcb74800d82bbebd17b1906e304"
     "a786f4f810c266c15be1a30576039c293272748d65966",
     "", "", "0", 0, 132, true},
    {15, IkeTestType::ikeV1AppBQuick,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "b10fff32cbeaa1e7afe6ab0b191e0bd63cd524849a4b56019146d232a24cf9af", "", "",
     "0", 0, 32, true},
    {16, IkeTestType::ikeV1AppBQuick,
     "5f00d1bd2c58ec224b1e6b71fa0f19a1faa7a193952c444411b47c1a9d8ba6fe", "", "",
     "b10fff32cbeaa1e7afe6ab0b191e0b", "", "", "0", 0, 15, true},
};

#endif  // ike_sha256_vectors_h__
