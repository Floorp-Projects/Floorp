/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling python3 genTestVectors.py */

#ifndef ike_aesxcbc_vectors_h__
#define ike_aesxcbc_vectors_h__

#include "testvectors_base/test-structs.h"

const IkeTestVector kIkeAesXcbcProofVectors[] = {
    // these vectors are self generated.
    {1, IkeTestType::ikeGxy,
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb", "", "",
     "ef41a18b8c1ece71d74fedb292fd0f00", "69a62284195f1680", "80c94ba25c8abda5",
     "", 0, 0, true},
    {2, IkeTestType::ikeV1, "ef41a18b8c1ece71d74fedb292fd0f00",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb", "",
     "13525f37f9db53a65d1945b9af2c94f4", "8c3bcd3a69831d7f", "d2d9a7ff4fbe95a7",
     "", 0, 0, true},
    {3, IkeTestType::ikeV1, "ef41a18b8c1ece71d74fedb292fd0f00",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb",
     "13525f37f9db53a65d1945b9af2c94f4", "39d0712a1a96d1afaddbc35de86bc404",
     "8c3bcd3a69831d7f", "d2d9a7ff4fbe95a7", "", 1, 0, true},
    {4, IkeTestType::ikeV1, "ef41a18b8c1ece71d74fedb292fd0f00",
     "8ba4cbc73c0187301dc19a975823854dbd641c597f637f8d053a83b9514673eb",
     "39d0712a1a96d1afaddbc35de86bc404", "691cc90e93feb1cc06c8d376d3188293",
     "8c3bcd3a69831d7f", "d2d9a7ff4fbe95a7", "", 2, 0, true},
    {5, IkeTestType::ikeV1Psk, "c0", "", "", "8963b0c6057c347c4ddec448f1779e2a",
     "03a6f25a83c8c2a3", "9d958a6618f77e7f", "", 0, 0, true},
    {6, IkeTestType::ikeGxy,
     "4b2c1f971981a8ad8d0abeafabf38cf75fc8349c148142465ed9c8b516b8be52", "", "",
     "08b95345c9557240ddc98d6e1dfda875", "32b50d5f4a3763f3", "9206a04b26564cb1",
     "", 0, 0, true},
    {7, IkeTestType::ikeV2Rekey, "efa38ecee9fd05062f64b655105436d54",
     "863f3c9d06efd39d2b907b97f8699e5dd5251ef64a2a176f36ee40c87d4f9330", "",
     "a881d193f5140415586a2839e1cacb91", "32b50d5f4a3763f3", "9206a04b26564cb1",
     "", 0, 0, true},
    {8, IkeTestType::ikePlus, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "efa38ecee9fd05062f64b655105436d54b4728da66f3bc5768636170ff5017ab082342a68"
     "3e7144a58d549c53d4575a2897d14c7c687040e86384065456b8dcd8aaea88b85b5e4d8ab"
     "2f61c015859337000550cda1750a15c1f90af0ddd296e0a7f291afe46295dd3108078bd8e"
     "adf09bc614c205a7c283907c3e6a384ad3f5373887e83",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0, 132, true},
    {9, IkeTestType::ikePlus, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "efa38ecee9fd05062f64b655105436d5", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0, 16, true},
    {10, IkeTestType::ikePlus, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "efa38ecee9fd05062f64b655105436", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "32b50d5f4a3763f3"   // Ni
     "9206a04b26564cb1"   // Nr
     "34c9e7c188868785"   // SPIi
     "3ff77d760d2b2199",  // SPIr
     0, 15, true},
    // these vectors are self-generated
    {11, IkeTestType::ikeV1AppB, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "9203190ea765285c14ec496acdb73f99479ee08f3e3b5f277a516439888f74a2ddb5023f2"
     "92c629e7194b3673632ff96bccd7de7ae68a90952fec65301c89d3a32981d5bb9d68b677e"
     "96703f34ed6474deee2d8aa5c5cee8997ec223a24cd537042b74d1b5274eebe76520481a7"
     "5a6d083b004819ea9359ffacef3ac6076cbbb0b80faab",
     "", "", "", 0, 132, true},
    {12, IkeTestType::ikeV1AppB, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "08b95345c9557240ddc98d6e1dfda875", "", "", "", 0, 16, true},
    {13, IkeTestType::ikeV1AppB, "08b95345c9557240ddc98d6e1dfda875", "", "",
     "08b95345c9557240ddc98d6e1dfda8", "", "", "", 0, 15, true},
    {14, IkeTestType::ikeV1AppBQuick, "08b95345c9557240ddc98d6e1dfda875", "",
     "",
     "9203190ea765285c14ec496acdb73f99a2358c44449799788d589fc426405bd0d9bc42758"
     "04e2946d3cfd6072db257e2da4b9fecca10f23b271f793e7f66d19db446245e6cdd9446a8"
     "e2ca27439c6692ce3f15cbcafc40c5879adb98310a4f8a5de14fe502d2c4e2b35f7054974"
     "9a95f9510ac2d02a470973ca91931f1a82bf944935f76",
     "", "", "0", 0, 132, true},
    {12, IkeTestType::ikeV1AppBQuick, "08b95345c9557240ddc98d6e1dfda875", "",
     "", "9203190ea765285c14ec496acdb73f99", "", "", "0", 0, 16, true},
    {16, IkeTestType::ikeV1AppBQuick, "08b95345c9557240ddc98d6e1dfda875", "",
     "", "9203190ea765285c14ec496acdb73f", "", "", "0", 0, 15, true},
};

#endif  // ike_aesxcbc_vectors_h__
