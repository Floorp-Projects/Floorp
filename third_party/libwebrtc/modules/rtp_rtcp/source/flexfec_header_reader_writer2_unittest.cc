/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/flexfec_header_reader_writer2.h"

#include "test/gtest.h"

namespace webrtc {

// TODO(bugs.webrtc.org/15002): reimplement all tests after changes
// to parser, and add tests for multi stream cases.

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit0Set) {}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit1Set) {}

TEST(FlexfecHeaderReader2Test, ReadsHeaderWithKBit2Set) {}

TEST(FlexfecHeaderReader2Test,
     ReadPacketWithoutStreamSpecificHeaderShouldFail) {}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit0SetShouldFail) {}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit1SetShouldFail) {}

TEST(FlexfecHeaderReader2Test, ReadShortPacketWithKBit2SetShouldFail) {}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit0Set) {}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit1Set) {}

TEST(FlexfecHeaderWriter2Test, FinalizesHeaderWithKBit2Set) {}

TEST(FlexfecHeaderWriter2Test, ContractsShortUlpfecPacketMaskWithBit15Clear) {}

TEST(FlexfecHeaderWriter2Test, ExpandsShortUlpfecPacketMaskWithBit15Set) {}

TEST(FlexfecHeaderWriter2Test,
     ContractsLongUlpfecPacketMaskWithBit46ClearBit47Clear) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Clear) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46ClearBit47Set) {}

TEST(FlexfecHeaderWriter2Test,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBit46SetBit47Clear) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderMaskWithBit46ClearBit47Set) {}

TEST(FlexfecHeaderReaderWriter2Test,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Set) {}

}  // namespace webrtc
