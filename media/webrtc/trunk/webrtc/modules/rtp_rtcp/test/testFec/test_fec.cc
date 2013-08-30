/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Test application for core FEC algorithm. Calls encoding and decoding
 * functions in ForwardErrorCorrection directly.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <list>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/source/fec_private_tables_bursty.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/testsupport/fileutils.h"

//#define VERBOSE_OUTPUT

namespace webrtc {
namespace test {

void ReceivePackets(
    ForwardErrorCorrection::ReceivedPacketList* toDecodeList,
    ForwardErrorCorrection::ReceivedPacketList* receivedPacketList,
    uint32_t numPacketsToDecode, float reorderRate, float duplicateRate) {
  assert(toDecodeList->empty());
  assert(numPacketsToDecode <= receivedPacketList->size());

  ForwardErrorCorrection::ReceivedPacketList::iterator it;
  for (uint32_t i = 0; i < numPacketsToDecode; i++) {
    it = receivedPacketList->begin();
    // Reorder packets.
    float randomVariable = static_cast<float>(rand()) / RAND_MAX;
    while (randomVariable < reorderRate) {
      ++it;
      if (it == receivedPacketList->end()) {
        --it;
        break;
      }
      randomVariable = static_cast<float>(rand()) / RAND_MAX;
    }
    ForwardErrorCorrection::ReceivedPacket* receivedPacket = *it;
    toDecodeList->push_back(receivedPacket);

    // Duplicate packets.
    randomVariable = static_cast<float>(rand()) / RAND_MAX;
    while (randomVariable < duplicateRate) {
      ForwardErrorCorrection::ReceivedPacket* duplicatePacket =
          new ForwardErrorCorrection::ReceivedPacket;
      *duplicatePacket = *receivedPacket;
      duplicatePacket->pkt = new ForwardErrorCorrection::Packet;
      memcpy(duplicatePacket->pkt->data, receivedPacket->pkt->data,
             receivedPacket->pkt->length);
      duplicatePacket->pkt->length = receivedPacket->pkt->length;

      toDecodeList->push_back(duplicatePacket);
      randomVariable = static_cast<float>(rand()) / RAND_MAX;
    }
    receivedPacketList->erase(it);
  }
}

TEST(FecTest, FecTest) {
  // TODO(marpan): Split this function into subroutines/helper functions.
  enum {
    kMaxNumberMediaPackets = 48
  };
  enum {
    kMaxNumberFecPackets = 48
  };

  const uint32_t kNumMaskBytesL0 = 2;
  const uint32_t kNumMaskBytesL1 = 6;

  // FOR UEP
  const bool kUseUnequalProtection = true;

  // FEC mask types.
  const FecMaskType kMaskTypes[] = { kFecMaskRandom, kFecMaskBursty };
  const int kNumFecMaskTypes = sizeof(kMaskTypes) / sizeof(*kMaskTypes);

  // TODO(pbos): Fix this. Hack to prevent a warning
  // ('-Wunneeded-internal-declaration') from clang.
  (void) kPacketMaskBurstyTbl;

  // Maximum number of media packets allowed for the mask type.
  const uint16_t kMaxMediaPackets[] = {kMaxNumberMediaPackets,
      sizeof(kPacketMaskBurstyTbl) / sizeof(*kPacketMaskBurstyTbl)};

  ASSERT_EQ(12, kMaxMediaPackets[1]) << "Max media packets for bursty mode not "
                                     << "equal to 12.";

  uint32_t id = 0;
  ForwardErrorCorrection fec(id);

  ForwardErrorCorrection::PacketList mediaPacketList;
  ForwardErrorCorrection::PacketList fecPacketList;
  ForwardErrorCorrection::ReceivedPacketList toDecodeList;
  ForwardErrorCorrection::ReceivedPacketList receivedPacketList;
  ForwardErrorCorrection::RecoveredPacketList recoveredPacketList;
  std::list<uint8_t*> fecMaskList;

  ForwardErrorCorrection::Packet* mediaPacket = NULL;
  // Running over only one loss rate to limit execution time.
  const float lossRate[] = { 0.5f };
  const uint32_t lossRateSize = sizeof(lossRate) / sizeof(*lossRate);
  const float reorderRate = 0.1f;
  const float duplicateRate = 0.1f;

  uint8_t mediaLossMask[kMaxNumberMediaPackets];
  uint8_t fecLossMask[kMaxNumberFecPackets];
  uint8_t fecPacketMasks[kMaxNumberFecPackets][kMaxNumberMediaPackets];

  // Seed the random number generator, storing the seed to file in order to
  // reproduce past results.
  const unsigned int randomSeed = static_cast<unsigned int>(time(NULL));
  srand(randomSeed);
  std::string filename = webrtc::test::OutputPath() + "randomSeedLog.txt";
  FILE* randomSeedFile = fopen(filename.c_str(), "a");
  fprintf(randomSeedFile, "%u\n", randomSeed);
  fclose(randomSeedFile);
  randomSeedFile = NULL;

  uint16_t seqNum = static_cast<uint16_t>(rand());
  uint32_t timeStamp = static_cast<uint32_t>(rand());
  const uint32_t ssrc = static_cast<uint32_t>(rand());

  // Loop over the mask types: random and bursty.
  for (int mask_type_idx = 0; mask_type_idx < kNumFecMaskTypes;
       ++mask_type_idx) {

    for (uint32_t lossRateIdx = 0; lossRateIdx < lossRateSize; ++lossRateIdx) {

      printf("Loss rate: %.2f, Mask type %d \n", lossRate[lossRateIdx],
             mask_type_idx);

      const uint32_t packetMaskMax = kMaxMediaPackets[mask_type_idx];
      uint8_t* packetMask = new uint8_t[packetMaskMax * kNumMaskBytesL1];

      FecMaskType fec_mask_type = kMaskTypes[mask_type_idx];

      for (uint32_t numMediaPackets = 1; numMediaPackets <= packetMaskMax;
           numMediaPackets++) {
        internal::PacketMaskTable mask_table(fec_mask_type, numMediaPackets);

        for (uint32_t numFecPackets = 1;
             numFecPackets <= numMediaPackets && numFecPackets <= packetMaskMax;
             numFecPackets++) {

          // Loop over numImpPackets: usually <= (0.3*numMediaPackets).
          // For this test we check up to ~ (0.5*numMediaPackets).
          uint32_t maxNumImpPackets = numMediaPackets / 2 + 1;
          for (uint32_t numImpPackets = 0; numImpPackets <= maxNumImpPackets &&
                                               numImpPackets <= packetMaskMax;
               numImpPackets++) {

            uint8_t protectionFactor =
                static_cast<uint8_t>(numFecPackets * 255 / numMediaPackets);

            const uint32_t maskBytesPerFecPacket =
                (numMediaPackets > 16) ? kNumMaskBytesL1 : kNumMaskBytesL0;

            memset(packetMask, 0, numMediaPackets * maskBytesPerFecPacket);

            // Transfer packet masks from bit-mask to byte-mask.
            internal::GeneratePacketMasks(numMediaPackets, numFecPackets,
                                          numImpPackets, kUseUnequalProtection,
                                          mask_table, packetMask);

#ifdef VERBOSE_OUTPUT
            printf("%u media packets, %u FEC packets, %u numImpPackets, "
                   "loss rate = %.2f \n",
                   numMediaPackets, numFecPackets, numImpPackets,
                   lossRate[lossRateIdx]);
            printf("Packet mask matrix \n");
#endif

            for (uint32_t i = 0; i < numFecPackets; i++) {
              for (uint32_t j = 0; j < numMediaPackets; j++) {
                const uint8_t byteMask =
                    packetMask[i * maskBytesPerFecPacket + j / 8];
                const uint32_t bitPosition = (7 - j % 8);
                fecPacketMasks[i][j] =
                    (byteMask & (1 << bitPosition)) >> bitPosition;
#ifdef VERBOSE_OUTPUT
                printf("%u ", fecPacketMasks[i][j]);
#endif
              }
#ifdef VERBOSE_OUTPUT
              printf("\n");
#endif
            }
#ifdef VERBOSE_OUTPUT
            printf("\n");
#endif
            // Check for all zero rows or columns: indicates incorrect mask.
            uint32_t rowLimit = numMediaPackets;
            for (uint32_t i = 0; i < numFecPackets; ++i) {
              uint32_t rowSum = 0;
              for (uint32_t j = 0; j < rowLimit; ++j) {
                rowSum += fecPacketMasks[i][j];
              }
              ASSERT_NE(0u, rowSum) << "Row is all zero " << i;
            }
            for (uint32_t j = 0; j < rowLimit; ++j) {
              uint32_t columnSum = 0;
              for (uint32_t i = 0; i < numFecPackets; ++i) {
                columnSum += fecPacketMasks[i][j];
              }
              ASSERT_NE(0u, columnSum) << "Column is all zero " << j;
            }

            // Construct media packets.
            for (uint32_t i = 0; i < numMediaPackets; ++i) {
              mediaPacket = new ForwardErrorCorrection::Packet;
              mediaPacketList.push_back(mediaPacket);
              mediaPacket->length = static_cast<uint16_t>(
                  (static_cast<float>(rand()) / RAND_MAX) *
                  (IP_PACKET_SIZE - 12 - 28 -
                   ForwardErrorCorrection::PacketOverhead()));
              if (mediaPacket->length < 12) {
                mediaPacket->length = 12;
              }
              // Generate random values for the first 2 bytes.
              mediaPacket->data[0] = static_cast<uint8_t>(rand() % 256);
              mediaPacket->data[1] = static_cast<uint8_t>(rand() % 256);

              // The first two bits are assumed to be 10 by the
              // FEC encoder. In fact the FEC decoder will set the
              // two first bits to 10 regardless of what they
              // actually were. Set the first two bits to 10
              // so that a memcmp can be performed for the
              // whole restored packet.
              mediaPacket->data[0] |= 0x80;
              mediaPacket->data[0] &= 0xbf;

              // FEC is applied to a whole frame.
              // A frame is signaled by multiple packets without
              // the marker bit set followed by the last packet of
              // the frame for which the marker bit is set.
              // Only push one (fake) frame to the FEC.
              mediaPacket->data[1] &= 0x7f;

              ModuleRTPUtility::AssignUWord16ToBuffer(&mediaPacket->data[2],
                                                      seqNum);
              ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[4],
                                                      timeStamp);
              ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[8],
                                                      ssrc);
              // Generate random values for payload
              for (int32_t j = 12; j < mediaPacket->length; ++j) {
                mediaPacket->data[j] = static_cast<uint8_t>(rand() % 256);
              }
              seqNum++;
            }
            mediaPacket->data[1] |= 0x80;

            ASSERT_EQ(0, fec.GenerateFEC(mediaPacketList, protectionFactor,
                                         numImpPackets, kUseUnequalProtection,
                                         fec_mask_type, &fecPacketList))
                << "GenerateFEC() failed";

            ASSERT_EQ(numFecPackets, fecPacketList.size())
                << "We requested " << numFecPackets << " FEC packets, but "
                << "GenerateFEC() produced " << fecPacketList.size();
            memset(mediaLossMask, 0, sizeof(mediaLossMask));
            ForwardErrorCorrection::PacketList::iterator mediaPacketListItem =
                mediaPacketList.begin();
            ForwardErrorCorrection::ReceivedPacket* receivedPacket;
            uint32_t mediaPacketIdx = 0;

            while (mediaPacketListItem != mediaPacketList.end()) {
              mediaPacket = *mediaPacketListItem;
              // We want a value between 0 and 1.
              const float lossRandomVariable =
                  (static_cast<float>(rand()) / (RAND_MAX));

              if (lossRandomVariable >= lossRate[lossRateIdx]) {
                mediaLossMask[mediaPacketIdx] = 1;
                receivedPacket = new ForwardErrorCorrection::ReceivedPacket;
                receivedPacket->pkt = new ForwardErrorCorrection::Packet;
                receivedPacketList.push_back(receivedPacket);

                receivedPacket->pkt->length = mediaPacket->length;
                memcpy(receivedPacket->pkt->data, mediaPacket->data,
                       mediaPacket->length);
                receivedPacket->seq_num =
                    ModuleRTPUtility::BufferToUWord16(&mediaPacket->data[2]);
                receivedPacket->is_fec = false;
              }
              mediaPacketIdx++;
              ++mediaPacketListItem;
            }
            memset(fecLossMask, 0, sizeof(fecLossMask));
            ForwardErrorCorrection::PacketList::iterator fecPacketListItem =
                fecPacketList.begin();
            ForwardErrorCorrection::Packet* fecPacket;
            uint32_t fecPacketIdx = 0;
            while (fecPacketListItem != fecPacketList.end()) {
              fecPacket = *fecPacketListItem;
              const float lossRandomVariable =
                  (static_cast<float>(rand()) / (RAND_MAX));
              if (lossRandomVariable >= lossRate[lossRateIdx]) {
                fecLossMask[fecPacketIdx] = 1;
                receivedPacket = new ForwardErrorCorrection::ReceivedPacket;
                receivedPacket->pkt = new ForwardErrorCorrection::Packet;

                receivedPacketList.push_back(receivedPacket);

                receivedPacket->pkt->length = fecPacket->length;
                memcpy(receivedPacket->pkt->data, fecPacket->data,
                       fecPacket->length);

                receivedPacket->seq_num = seqNum;
                receivedPacket->is_fec = true;
                receivedPacket->ssrc = ssrc;

                fecMaskList.push_back(fecPacketMasks[fecPacketIdx]);
              }
              ++fecPacketIdx;
              ++seqNum;
              ++fecPacketListItem;
            }

#ifdef VERBOSE_OUTPUT
            printf("Media loss mask:\n");
            for (uint32_t i = 0; i < numMediaPackets; i++) {
              printf("%u ", mediaLossMask[i]);
            }
            printf("\n\n");

            printf("FEC loss mask:\n");
            for (uint32_t i = 0; i < numFecPackets; i++) {
              printf("%u ", fecLossMask[i]);
            }
            printf("\n\n");
#endif

            std::list<uint8_t*>::iterator fecMaskIt = fecMaskList.begin();
            uint8_t* fecMask;
            while (fecMaskIt != fecMaskList.end()) {
              fecMask = *fecMaskIt;
              uint32_t hammingDist = 0;
              uint32_t recoveryPosition = 0;
              for (uint32_t i = 0; i < numMediaPackets; i++) {
                if (mediaLossMask[i] == 0 && fecMask[i] == 1) {
                  recoveryPosition = i;
                  ++hammingDist;
                }
              }
              std::list<uint8_t*>::iterator itemToDelete = fecMaskIt;
              ++fecMaskIt;

              if (hammingDist == 1) {
                // Recovery possible. Restart search.
                mediaLossMask[recoveryPosition] = 1;
                fecMaskIt = fecMaskList.begin();
              } else if (hammingDist == 0) {
                // FEC packet cannot provide further recovery.
                fecMaskList.erase(itemToDelete);
              }
            }
#ifdef VERBOSE_OUTPUT
            printf("Recovery mask:\n");
            for (uint32_t i = 0; i < numMediaPackets; ++i) {
              printf("%u ", mediaLossMask[i]);
            }
            printf("\n\n");
#endif
            // For error-checking frame completion.
            bool fecPacketReceived = false;
            while (!receivedPacketList.empty()) {
              uint32_t numPacketsToDecode = static_cast<uint32_t>(
                  (static_cast<float>(rand()) / RAND_MAX) *
                      receivedPacketList.size() + 0.5);
              if (numPacketsToDecode < 1) {
                numPacketsToDecode = 1;
              }
              ReceivePackets(&toDecodeList, &receivedPacketList,
                             numPacketsToDecode, reorderRate, duplicateRate);

              if (fecPacketReceived == false) {
                ForwardErrorCorrection::ReceivedPacketList::iterator
                toDecodeIt = toDecodeList.begin();
                while (toDecodeIt != toDecodeList.end()) {
                  receivedPacket = *toDecodeIt;
                  if (receivedPacket->is_fec) {
                    fecPacketReceived = true;
                  }
                  ++toDecodeIt;
                }
              }
              ASSERT_EQ(0, fec.DecodeFEC(&toDecodeList, &recoveredPacketList))
                  << "DecodeFEC() failed";
              ASSERT_TRUE(toDecodeList.empty())
                  << "Received packet list is not empty.";
            }
            mediaPacketListItem = mediaPacketList.begin();
            mediaPacketIdx = 0;
            while (mediaPacketListItem != mediaPacketList.end()) {
              if (mediaLossMask[mediaPacketIdx] == 1) {
                // Should have recovered this packet.
                ForwardErrorCorrection::RecoveredPacketList::iterator
                recoveredPacketListItem = recoveredPacketList.begin();

                ASSERT_FALSE(
                    recoveredPacketListItem == recoveredPacketList.end())
                  << "Insufficient number of recovered packets.";
                mediaPacket = *mediaPacketListItem;
                ForwardErrorCorrection::RecoveredPacket* recoveredPacket =
                    *recoveredPacketListItem;

                ASSERT_EQ(recoveredPacket->pkt->length, mediaPacket->length)
                    << "Recovered packet length not identical to original "
                    << "media packet";
                ASSERT_EQ(0, memcmp(recoveredPacket->pkt->data,
                                    mediaPacket->data, mediaPacket->length))
                    << "Recovered packet payload not identical to original "
                    << "media packet";
                delete recoveredPacket;
                recoveredPacketList.pop_front();
              }
              ++mediaPacketIdx;
              ++mediaPacketListItem;
            }
            fec.ResetState(&recoveredPacketList);
            ASSERT_TRUE(recoveredPacketList.empty())
                << "Excessive number of recovered packets.\t size is: "
                << recoveredPacketList.size();
            // -- Teardown --
            mediaPacketListItem = mediaPacketList.begin();
            while (mediaPacketListItem != mediaPacketList.end()) {
              delete *mediaPacketListItem;
              ++mediaPacketListItem;
              mediaPacketList.pop_front();
            }
            assert(mediaPacketList.empty());

            fecPacketListItem = fecPacketList.begin();
            while (fecPacketListItem != fecPacketList.end()) {
              ++fecPacketListItem;
              fecPacketList.pop_front();
            }

            // Delete received packets we didn't pass to DecodeFEC(), due to
            // early frame completion.
            ForwardErrorCorrection::ReceivedPacketList::iterator
            receivedPacketIt = receivedPacketList.begin();
            while (receivedPacketIt != receivedPacketList.end()) {
              receivedPacket = *receivedPacketIt;
              delete receivedPacket;
              ++receivedPacketIt;
              receivedPacketList.pop_front();
            }
            assert(receivedPacketList.empty());

            while (!fecMaskList.empty()) {
              fecMaskList.pop_front();
            }
            timeStamp += 90000 / 30;
          }  // loop over numImpPackets
        }    // loop over FecPackets
      }      // loop over numMediaPackets
      delete[] packetMask;
    }  // loop over loss rates
  }    // loop over mask types

  // Have DecodeFEC free allocated memory.
  fec.ResetState(&recoveredPacketList);
  ASSERT_TRUE(recoveredPacketList.empty())
      << "Recovered packet list is not empty";
}

}  // namespace test
}  // namespace webrtc
