/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling python3 genTestVectors.py */

#ifndef ike_sha1_vectors_h__
#define ike_sha1_vectors_h__

#include "testvectors_base/test-structs.h"

const IkeTestVector kIkeSha1ProofVectors[] = {
    // these vectors are from this NIST samples
    {1, IkeTestType::ikeGxy,
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb", "", "",
     "707197817fb2d90cf54d1842606bdea59b9f4823", "69a62284195f1680",
     "80c94ba25c8abda5", "", 0, 0, true},
    {2, IkeTestType::ikeV1, "707197817fb2d90cf54d1842606bdea59b9f4823",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb", "",
     "384be709a8a5e63c3ed160cfe3921c4b37d5b32d", "8c3bcd3a69831d7f",
     "d2d9a7ff4fbe95a7", "", 0, 0, true},
    {3, IkeTestType::ikeV1, "707197817fb2d90cf54d1842606bdea59b9f4823",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb",
     "384be709a8a5e63c3ed160cfe3921c4b37d5b32d",
     "48b327575abe3adba0f279849e289022a13e2b47", "8c3bcd3a69831d7f",
     "d2d9a7ff4fbe95a7", "", 1, 0, true},
    {4, IkeTestType::ikeV1, "707197817fb2d90cf54d1842606bdea59b9f4823",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb",
     "48b327575abe3adba0f279849e289022a13e2b47",
     "a4a415c8e0c38c0da847c356cc61c24df8025560", "8c3bcd3a69831d7f",
     "d2d9a7ff4fbe95a7", "", 2, 0, true},
    {5, IkeTestType::ikeV1Psk, "c0", "", "",
     "ab3be41bc62f2ef0c41a3076d58768be77fadd2e", "03a6f25a83c8c2a3",
     "9d958a6618f77e7f", "", 0, 0, true},
    {6, IkeTestType::ikeGxy,
     "4b2c1f971981a8ad8d0abeafabf38cf75fc8349c148142465ed9c8b516b8be52", "", "",
     "a9a7b222b59f8f48645f28a1db5b5f5d7479cba7", "32b50d5f4a3763f3",
     "9206a04b26564cb1", "", 0, 0, true},
    {7, IkeTestType::ikeV2Rekey, "a14293677cc80ff8f9cc0eee30d895da9d8f4056",
     "863f3c9d06efd39d2b907b97f8699e5dd5251ef64a2a176f36ee40c87d4f9330", "",
     "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "32b50d5f4a3763f3",
     "9206a04b26564cb1", "", 0, 0, true},
    {8, IkeTestType::ikePlus, "a9a7b222b59f8f48645f28a1db5b5f5d7479cba7", "",
     "",
     "a14293677cc80ff8f9cc0eee30d895da9d8f405666e30ef0dfcb63c634a46002a2a63080e"
     "514a062768b76606f9fa5e992204fc5a670bde3f10d6b027113936a5c55b648a194ae587b"
     "0088d52204b702c979fa280870d2ed41efa9c549fd11198af1670b143d384bd275c5f594c"
     "f266b05ebadca855e4249520a441a81157435a7a56cc4",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0,
     132, true},
    {9, IkeTestType::ikePlus, "a9a7b222b59f8f48645f28a1db5b5f5d7479cba7", "",
     "",
     "a14293677cc80ff8f9cc0eee30d895da9d8f405666e30ef0dfcb63c634a46002a2a63080e"
     "514a062",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0,
     40, true},
    {10, IkeTestType::ikePlus, "a9a7b222b59f8f48645f28a1db5b5f5d7479cba7", "",
     "", "a14293677cc80ff8f9cc0eee30d895", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0,
     15, true},
    // these vectors are self-generated
    {11, IkeTestType::ikeV1AppB, "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "",
     "",
     "933347a07de5782247dd36d1562ffe0eecade1eb4134165257e3af1000af8ae3f16506382"
     "8cbb60d910b7db38fa3c7f62c4afaaf3203da065c841729853edb23e9e7ac8286ae65c8cb"
     "6c667d79268c0bd6705abb9131698eb822b1c1f9dd142fc7be2c1010ee0152e10195add98"
     "999c6b6d42c8fe9c1b134d56ad5f2c6f20e815bd25c52",
     "", "", "", 0, 132, true},
    {12, IkeTestType::ikeV1AppB, "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "",
     "",
     "933347a07de5782247dd36d1562ffe0eecade1eb4134165257e3af1000af8ae3f16506382"
     "8cbb60d",
     "", "", "", 0, 40, true},
    {13, IkeTestType::ikeV1AppB, "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "",
     "", "63e81194946ebd05df7df5ebf5d875", "", "", "", 0, 15, true},
    {14, IkeTestType::ikeV1AppBQuick,
     "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "", "",
     "933347a07de5782247dd36d1562ffe0eecade1ebaeaa476a5f578c34a9b2b7101a621202f"
     "61db924c5ef9efa3bb2698095841603b7ac8a880329a927ecd4ad53a944b607a5ac2f3d15"
     "4e2748c188d7370d76be83fc204fdacf0f66b99dd760ba619ffac65eda1420c8a936dac5a"
     "599afaf4043b29ef2b65dc042724355b550875316c6fd",
     "", "", "0", 0, 132, true},
    {15, IkeTestType::ikeV1AppBQuick,
     "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "", "",
     "933347a07de5782247dd36d1562ffe0eecade1ebaeaa476a5f578c34a9b2b7101a621202f"
     "61db924",
     "", "", "0", 0, 40, true},
    {16, IkeTestType::ikeV1AppBQuick,
     "63e81194946ebd05df7df5ebf5d8750056bf1f1d", "", "",
     "933347a07de5782247dd36d1562ffe", "", "", "0", 0, 15, true},
};

#endif  // ike_sha1_vectors_h__
