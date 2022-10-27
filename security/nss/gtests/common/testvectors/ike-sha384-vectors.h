/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file is generated from sources in nss/gtests/common/wycheproof
 * automatically and should not be touched manually.
 * Generation is trigged by calling python3 genTestVectors.py */

#ifndef ike_sha384_vectors_h__
#define ike_sha384_vectors_h__

#include "testvectors_base/test-structs.h"

const IkeTestVector kIkeSha384ProofVectors[] = {
    // these vectors are from this NIST samples
    {1, IkeTestType::ikeGxy,
     "1724dbd893523764bfef8c6fa927856fccfb77ae254358cce29c2769a32915c1", "", "",
     "6e4514610bf82d0ab7bf0260096f6146a153c712071abb633ced813c572156c783e36874a"
     "65a64690ca701d40d56ea18",
     "cec89d845add83ef", "cebd43ab71d17db9", "", 0, 0, true},
    {2, IkeTestType::ikeV1,
     "6e4514610bf82d0ab7bf0260096f6146a153c712071abb633ced813c572156c783e36874a"
     "65a64690ca701d40d56ea18",
     "1724dbd893523764bfef8c6fa927856fccfb77ae254358cce29c2769a32915c1", "",
     "b083234e9ed7745911f93eb31faa66fcf88906266830eb17ef166d295cb1f86a3543b8b8e"
     "fa5df918533df537e9c809c",
     "1c8aba986a00af0f", "b049d9672f73c920", "", 0, 0, true},
    {3, IkeTestType::ikeV1,
     "6e4514610bf82d0ab7bf0260096f6146a153c712071abb633ced813c572156c783e36874a"
     "65a64690ca701d40d56ea18",
     "1724dbd893523764bfef8c6fa927856fccfb77ae254358cce29c2769a32915c1",
     "b083234e9ed7745911f93eb31faa66fcf88906266830eb17ef166d295cb1f86a3543b8b8e"
     "fa5df918533df537e9c809c",
     "938295a374aceb4147a8024c9a007dd313403fd8fd7070dbd0cfbe1ccd308dbfbb7b9e9c6"
     "4049e4df44ff551016cb7b5",
     "1c8aba986a00af0f", "b049d9672f73c920", "", 1, 0, true},
    {4, IkeTestType::ikeV1,
     "6e4514610bf82d0ab7bf0260096f6146a153c712071abb633ced813c572156c783e36874a"
     "65a64690ca701d40d56ea18",
     "1724dbd893523764bfef8c6fa927856fccfb77ae254358cce29c2769a32915c1",
     "938295a374aceb4147a8024c9a007dd313403fd8fd7070dbd0cfbe1ccd308dbfbb7b9e9c6"
     "4049e4df44ff551016cb7b5",
     "8595b249dc1fa8599729f87eb6b9dd13bfbfdfd4f9ebd78929bab6ecc402539ad32cb6e7e"
     "f4ba6a0f53da14e4df07ed4",
     "1c8aba986a00af0f", "b049d9672f73c920", "", 2, 0, true},
    {5, IkeTestType::ikeV1Psk, "9e", "", "",
     "b54fa27cb4251051e44a659d73591845691d11f1874bf4e4088e5df6462d28e57a3a2af3a"
     "b4f9b746a8f5766f8785f2b",
     "d6596b7e5b398534", "136fbdfa8d0ceb8e", "", 0, 0, true},
    {6, IkeTestType::ikeGxy,
     "d3288cd87565101e88fe3bad918f31939d8dd26ff1071f8b2d7f447524e58d7c", "", "",
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "fd1b572a8e735591", "6013b0ef88dacd3d", "", 0, 0, true},
    {7, IkeTestType::ikeV2Rekey,
     "4f904c2025c90c817ea5ff9b662a6fdb445a73b57cdf09eacd379b95e1f03cacb04cd6dee"
     "da4f952191dd9bc1f7a9502",
     "3358f620539473aee8d07e779764c4c6a9aabddc79a28e136b3bac021dbde44a", "",
     "e0548c1682e13bce454026b3b1bdf42985b24e4e7408095a7c529de38c3d1fcb04c9fe686"
     "8042a34c9614c6c99e3fcea",
     "fd1b572a8e735591", "6013b0ef88dacd3d", "", 0, 0, true},
    {8, IkeTestType::ikePlus,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "4f904c2025c90c817ea5ff9b662a6fdb445a73b57cdf09eacd379b95e1f03cacb04cd6dee"
     "da4f952191dd9bc1f7a9502471a648d74dc06d38112de48a42501f6b1a3ad55c2099cd9a6"
     "48e5f17e5bf3e34bf9b5953decb768a34f875fe2b78dca0c2fcca81ec1a412006dfaed38f"
     "a06882e61f4c148105fb8e231fdb33c4d484c001721d4",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "fd1b572a8e735591"   // Ni
     "6013b0ef88dacd3d"   // Nr
     "2116ad07ce61f749"   // SPIi
     "24880e55f11a65b7",  // SPIr
     0, 132, true},
    {9, IkeTestType::ikePlus,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "4f904c2025c90c817ea5ff9b662a6fdb445a73b57cdf09eacd379b95e1f03cacb04cd6dee"
     "da4f952191dd9bc1f7a9502",
     "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "fd1b572a8e735591"   // Ni
     "6013b0ef88dacd3d"   // Nr
     "2116ad07ce61f749"   // SPIi
     "24880e55f11a65b7",  // SPIr
     0, 48, true},
    {10, IkeTestType::ikePlus,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "", "4f904c2025c90c817ea5ff9b662a6f", "", "",
     // seed_data is Ni || Nr || SPIi || SPIr
     // NOTE: there is no comma so the strings are concatenated together.
     "fd1b572a8e735591"   // Ni
     "6013b0ef88dacd3d"   // Nr
     "2116ad07ce61f749"   // SPIi
     "24880e55f11a65b7",  // SPIr
     0, 15, true},
    // these vectors are self-generated
    {11, IkeTestType::ikeV1AppB,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "9b9a56a512cc2c5d5bcee66d03974f2701d4634b3241df132b1d2fd31fb23f003969dd787"
     "3425f771aae298871672cbfc908596c4d18165331b9fdff350cff787e700a140e123f2066"
     "d8d8527f53e701d23abdb3b0bc713109e33dc233c6989fa64b95720495c859505c5c7a748"
     "7778aab59365dafe60c7264ccde55829f60143a4bb095",
     "", "", "", 0, 132, true},
    {12, IkeTestType::ikeV1AppB,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "", "", 0, 48, true},
    {13, IkeTestType::ikeV1AppB,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "", "69fe7a1ac94adaeb711295f5fe004b", "", "", "", 0, 15, true},
    {14, IkeTestType::ikeV1AppBQuick,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "9b9a56a512cc2c5d5bcee66d03974f2701d4634b3241df132b1d2fd31fb23f003969dd787"
     "3425f771aae298871672cbf0e0b966f3e961d3d94c2205decc285afae5aad6abe9ca6f5fb"
     "8420fb940bc7760c63c45bd577f561f3643fc98bff8e26663f40f225865e79cca504f527f"
     "abcfc24bd1ba8e2dbd022120f0fd9fb2caa28b031607b",
     "", "", "0", 0, 132, true},
    {15, IkeTestType::ikeV1AppBQuick,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "",
     "9b9a56a512cc2c5d5bcee66d03974f2701d4634b3241df132b1d2fd31fb23f003969dd787"
     "3425f771aae298871672cbf",
     "", "", "0", 0, 48, true},
    {16, IkeTestType::ikeV1AppBQuick,
     "69fe7a1ac94adaeb711295f5fe004b1a8d6a0b65d05692758ce8ad2f7a45f59d7d0b596f5"
     "1f7dfcf3330061888f6a94f",
     "", "", "9b9a56a512cc2c5d5bcee66d03974f", "", "", "0", 0, 15, true},
};

#endif  // ike_sha384_vectors_h__
