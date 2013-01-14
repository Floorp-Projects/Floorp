/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"

#include <cassert>
#include <cstring>

#include "modules/rtp_rtcp/source/fec_private_tables_random.h"
#include "modules/rtp_rtcp/source/fec_private_tables_bursty.h"

namespace {

// Allow for different modes of protection for packets in UEP case.
enum ProtectionMode
{
    kModeNoOverlap,
    kModeOverlap,
    kModeBiasFirstPacket,
};

/**
  * Fits an input mask (subMask) to an output mask.
  * The mask is a matrix where the rows are the FEC packets,
  * and the columns are the source packets the FEC is applied to.
  * Each row of the mask is represented by a number of mask bytes.
  *
  * \param[in]  numMaskBytes    The number of mask bytes of output mask.
  * \param[in]  numSubMaskBytes The number of mask bytes of input mask.
  * \param[in]  numRows         The number of rows of the input mask.
  * \param[in]  subMask         A pointer to hold the input mask, of size
  *                             [0, numRows * numSubMaskBytes]
  * \param[out] packetMask      A pointer to hold the output mask, of size
  *                             [0, x * numMaskBytes], where x >= numRows.
  */
void FitSubMask(int numMaskBytes,
                int numSubMaskBytes,
                int numRows,
                const uint8_t* subMask,
                uint8_t* packetMask)
{
    if (numMaskBytes == numSubMaskBytes)
    {
        memcpy(packetMask, subMask, numRows * numSubMaskBytes);
    }
    else
    {
        for (int i = 0; i < numRows; i++)
        {
            int pktMaskIdx = i * numMaskBytes;
            int pktMaskIdx2 = i * numSubMaskBytes;
            for (int j = 0; j < numSubMaskBytes; j++)
            {
                packetMask[pktMaskIdx] = subMask[pktMaskIdx2];
                pktMaskIdx++;
                pktMaskIdx2++;
            }
        }
    }
}

/**
  * Shifts a mask by number of columns (bits), and fits it to an output mask.
  * The mask is a matrix where the rows are the FEC packets,
  * and the columns are the source packets the FEC is applied to.
  * Each row of the mask is represented by a number of mask bytes.
  *
  * \param[in]  numMaskBytes     The number of mask bytes of output mask.
  * \param[in]  numSubMaskBytes  The number of mask bytes of input mask.
  * \param[in]  numColumnShift   The number columns to be shifted, and
  *                              the starting row for the output mask.
  * \param[in]  endRow           The ending row for the output mask.
  * \param[in]  subMask          A pointer to hold the input mask, of size
  *                              [0, (endRowFEC - startRowFec) * numSubMaskBytes]
  * \param[out] packetMask       A pointer to hold the output mask, of size
  *                              [0, x * numMaskBytes], where x >= endRowFEC.
  */
// TODO (marpan): This function is doing three things at the same time:
// shift within a byte, byte shift and resizing.
// Split up into subroutines.
void ShiftFitSubMask(int numMaskBytes,
                     int resMaskBytes,
                     int numColumnShift,
                     int endRow,
                     const uint8_t* subMask,
                     uint8_t* packetMask)
{

    // Number of bit shifts within a byte
    const int numBitShifts = (numColumnShift % 8);
    const int numByteShifts = numColumnShift >> 3;

    // Modify new mask with sub-mask21.

    // Loop over the remaining FEC packets.
    for (int i = numColumnShift; i < endRow; i++)
    {
        // Byte index of new mask, for row i and column resMaskBytes,
        // offset by the number of bytes shifts
        int pktMaskIdx = i * numMaskBytes + resMaskBytes - 1 + numByteShifts;
        // Byte index of subMask, for row i and column resMaskBytes
        int pktMaskIdx2 =
            (i - numColumnShift) * resMaskBytes + resMaskBytes - 1;

        uint8_t shiftRightCurrByte = 0;
        uint8_t shiftLeftPrevByte = 0;
        uint8_t combNewByte = 0;

        // Handle case of numMaskBytes > resMaskBytes:
        // For a given row, copy the rightmost "numBitShifts" bits
        // of the last byte of subMask into output mask.
        if (numMaskBytes > resMaskBytes)
        {
            shiftLeftPrevByte =
                (subMask[pktMaskIdx2] << (8 - numBitShifts));
            packetMask[pktMaskIdx + 1] = shiftLeftPrevByte;
        }

        // For each row i (FEC packet), shift the bit-mask of the subMask.
        // Each row of the mask contains "resMaskBytes" of bytes.
        // We start from the last byte of the subMask and move to first one.
        for (int j = resMaskBytes - 1; j > 0; j--)
        {
            // Shift current byte of sub21 to the right by "numBitShifts".
            shiftRightCurrByte =
                subMask[pktMaskIdx2] >> numBitShifts;

            // Fill in shifted bits with bits from the previous (left) byte:
            // First shift the previous byte to the left by "8-numBitShifts".
            shiftLeftPrevByte =
                (subMask[pktMaskIdx2 - 1] << (8 - numBitShifts));

            // Then combine both shifted bytes into new mask byte.
            combNewByte = shiftRightCurrByte | shiftLeftPrevByte;

            // Assign to new mask.
            packetMask[pktMaskIdx] = combNewByte;
            pktMaskIdx--;
            pktMaskIdx2--;
        }
        // For the first byte in the row (j=0 case).
        shiftRightCurrByte = subMask[pktMaskIdx2] >> numBitShifts;
        packetMask[pktMaskIdx] = shiftRightCurrByte;

    }
}
}  // namespace

namespace webrtc {
namespace internal {

PacketMaskTable::PacketMaskTable(FecMaskType fec_mask_type,
                                 int num_media_packets)
    : fec_mask_type_(InitMaskType(fec_mask_type, num_media_packets)),
      fec_packet_mask_table_(InitMaskTable(fec_mask_type_)) {
}

// Sets |fec_mask_type_| to the type of packet mask selected. The type of
// packet mask selected is based on |fec_mask_type| and |numMediaPackets|.
// If |numMediaPackets| is larger than the maximum allowed by |fec_mask_type|
// for the bursty type, then the random type is selected.
FecMaskType PacketMaskTable::InitMaskType(FecMaskType fec_mask_type,
                                          int num_media_packets) {
  // The mask should not be bigger than |packetMaskTbl|.
  assert(num_media_packets <= static_cast<int>(sizeof(kPacketMaskRandomTbl) /
                                               sizeof(*kPacketMaskRandomTbl)));
  switch (fec_mask_type) {
    case kFecMaskRandom: {
      return kFecMaskRandom;
    }
    case kFecMaskBursty: {
      int max_media_packets = static_cast<int>(sizeof(kPacketMaskBurstyTbl) /
                                               sizeof(*kPacketMaskBurstyTbl));
      if (num_media_packets > max_media_packets) {
        return kFecMaskRandom;
      } else {
        return kFecMaskBursty;
      }
    }
  }
  assert(false);
  return kFecMaskRandom;
}

// Returns the pointer to the packet mask tables corresponding to type
// |fec_mask_type|.
const uint8_t*** PacketMaskTable::InitMaskTable(FecMaskType fec_mask_type) {
  switch (fec_mask_type) {
    case kFecMaskRandom: {
      return kPacketMaskRandomTbl;
    }
    case kFecMaskBursty: {
      return kPacketMaskBurstyTbl;
    }
  }
  assert(false);
  return kPacketMaskRandomTbl;
}

// Remaining protection after important (first partition) packet protection
void RemainingPacketProtection(int numMediaPackets,
                               int numFecRemaining,
                               int numFecForImpPackets,
                               int numMaskBytes,
                               ProtectionMode mode,
                               uint8_t* packetMask,
                               const PacketMaskTable& mask_table)
{
    if (mode == kModeNoOverlap)
    {
        // subMask21

        const int lBit =
            (numMediaPackets - numFecForImpPackets) > 16 ? 1 : 0;

        const int resMaskBytes =
            (lBit == 1) ? kMaskSizeLBitSet : kMaskSizeLBitClear;

        const uint8_t* packetMaskSub21 =
            mask_table.fec_packet_mask_table()
            [numMediaPackets - numFecForImpPackets - 1]
            [numFecRemaining - 1];

        ShiftFitSubMask(numMaskBytes, resMaskBytes, numFecForImpPackets,
                        (numFecForImpPackets + numFecRemaining),
                        packetMaskSub21, packetMask);

    }
    else if (mode == kModeOverlap || mode == kModeBiasFirstPacket)
    {
        // subMask22

        const uint8_t* packetMaskSub22 =
            mask_table.fec_packet_mask_table()
            [numMediaPackets - 1][numFecRemaining - 1];

        FitSubMask(numMaskBytes, numMaskBytes, numFecRemaining, packetMaskSub22,
                   &packetMask[numFecForImpPackets * numMaskBytes]);

        if (mode == kModeBiasFirstPacket)
        {
            for (int i = 0; i < numFecRemaining; i++)
            {
                int pktMaskIdx = i * numMaskBytes;
                packetMask[pktMaskIdx] = packetMask[pktMaskIdx] | (1 << 7);
            }
        }
    }
    else
    {
        assert(false);
    }

}

// Protection for important (first partition) packets
void ImportantPacketProtection(int numFecForImpPackets,
                               int numImpPackets,
                               int numMaskBytes,
                               uint8_t* packetMask,
                               const PacketMaskTable& mask_table)
{
    const int lBit = numImpPackets > 16 ? 1 : 0;
    const int numImpMaskBytes =
        (lBit == 1) ? kMaskSizeLBitSet : kMaskSizeLBitClear;

    // Get subMask1 from table
    const uint8_t* packetMaskSub1 =
        mask_table.fec_packet_mask_table()
        [numImpPackets - 1][numFecForImpPackets - 1];

    FitSubMask(numMaskBytes, numImpMaskBytes,
               numFecForImpPackets, packetMaskSub1, packetMask);

}

// This function sets the protection allocation: i.e., how many FEC packets
// to use for numImp (1st partition) packets, given the: number of media
// packets, number of FEC packets, and number of 1st partition packets.
int SetProtectionAllocation(int numMediaPackets,
                            int numFecPackets,
                            int numImpPackets)
{

    // TODO (marpan): test different cases for protection allocation:

    // Use at most (allocPar * numFecPackets) for important packets.
    float allocPar = 0.5;
    int maxNumFecForImp = allocPar * numFecPackets;

    int numFecForImpPackets = (numImpPackets < maxNumFecForImp) ?
        numImpPackets : maxNumFecForImp;

    // Fall back to equal protection in this case
    if (numFecPackets == 1 && (numMediaPackets > 2 * numImpPackets))
    {
        numFecForImpPackets = 0;
    }

    return numFecForImpPackets;
}

// Modification for UEP: reuse the off-line tables for the packet masks.
// Note: these masks were designed for equal packet protection case,
// assuming random packet loss.

// Current version has 3 modes (options) to build UEP mask from existing ones.
// Various other combinations may be added in future versions.
// Longer-term, we may add another set of tables specifically for UEP cases.
// TODO (marpan): also consider modification of masks for bursty loss cases.

// Mask is characterized as (#packets_to_protect, #fec_for_protection).
// Protection factor defined as: (#fec_for_protection / #packets_to_protect).

// Let k=numMediaPackets, n=total#packets, (n-k)=numFecPackets, m=numImpPackets.

// For ProtectionMode 0 and 1:
// one mask (subMask1) is used for 1st partition packets,
// the other mask (subMask21/22, for 0/1) is for the remaining FEC packets.

// In both mode 0 and 1, the packets of 1st partition (numImpPackets) are
// treated equally important, and are afforded more protection than the
// residual partition packets.

// For numImpPackets:
// subMask1 = (m, t): protection = t/(m), where t=F(k,n-k,m).
// t=F(k,n-k,m) is the number of packets used to protect first partition in
// subMask1. This is determined from the function SetProtectionAllocation().

// For the left-over protection:
// Mode 0: subMask21 = (k-m,n-k-t): protection = (n-k-t)/(k-m)
// mode 0 has no protection overlap between the two partitions.
// For mode 0, we would typically set t = min(m, n-k).


// Mode 1: subMask22 = (k, n-k-t), with protection (n-k-t)/(k)
// mode 1 has protection overlap between the two partitions (preferred).

// For ProtectionMode 2:
// This gives 1st packet of list (which is 1st packet of 1st partition) more
// protection. In mode 2, the equal protection mask (which is obtained from
// mode 1 for t=0) is modified (more "1s" added in 1st column of packet mask)
// to bias higher protection for the 1st source packet.

// Protection Mode 2 may be extended for a sort of sliding protection
// (i.e., vary the number/density of "1s" across columns) across packets.

void UnequalProtectionMask(int numMediaPackets,
                           int numFecPackets,
                           int numImpPackets,
                           int numMaskBytes,
                           uint8_t* packetMask,
                           const PacketMaskTable& mask_table)
{

    // Set Protection type and allocation
    // TODO (marpan): test/update for best mode and some combinations thereof.

    ProtectionMode mode = kModeOverlap;
    int numFecForImpPackets = 0;

    if (mode != kModeBiasFirstPacket)
    {
        numFecForImpPackets = SetProtectionAllocation(numMediaPackets,
                                                      numFecPackets,
                                                      numImpPackets);
    }

    int numFecRemaining = numFecPackets - numFecForImpPackets;
    // Done with setting protection type and allocation

    //
    // Generate subMask1
    //
    if (numFecForImpPackets > 0)
    {
        ImportantPacketProtection(numFecForImpPackets, numImpPackets,
                                  numMaskBytes, packetMask,
                                  mask_table);
    }

    //
    // Generate subMask2
    //
    if (numFecRemaining > 0)
    {
        RemainingPacketProtection(numMediaPackets, numFecRemaining,
                                  numFecForImpPackets, numMaskBytes,
                                  mode, packetMask, mask_table);
    }

}

void GeneratePacketMasks(int numMediaPackets,
                         int numFecPackets,
                         int numImpPackets,
                         bool useUnequalProtection,
                         const PacketMaskTable& mask_table,
                         uint8_t* packetMask)
{
    assert(numMediaPackets > 0);
    assert(numFecPackets <= numMediaPackets && numFecPackets > 0);
    assert(numImpPackets <= numMediaPackets && numImpPackets >= 0);

    int lBit = numMediaPackets > 16 ? 1 : 0;
    const int numMaskBytes =
        (lBit == 1) ? kMaskSizeLBitSet : kMaskSizeLBitClear;

    // Equal-protection for these cases
    if (!useUnequalProtection || numImpPackets == 0)
    {
        // Retrieve corresponding mask table directly:for equal-protection case.
        // Mask = (k,n-k), with protection factor = (n-k)/k,
        // where k = numMediaPackets, n=total#packets, (n-k)=numFecPackets.
        memcpy(packetMask,
               mask_table.fec_packet_mask_table()[numMediaPackets - 1]
                                                 [numFecPackets - 1],
               numFecPackets * numMaskBytes);
    }
    else  //UEP case
    {
        UnequalProtectionMask(numMediaPackets, numFecPackets, numImpPackets,
                              numMaskBytes, packetMask, mask_table);

    } // End of UEP modification
} //End of GetPacketMasks

}  // namespace internal
}  // namespace webrtc
