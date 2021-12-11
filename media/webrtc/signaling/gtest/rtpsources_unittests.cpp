#include <RtpSourceObserver.h>
#include "RTCStatsReport.h"
#include "webrtc/modules/include/module_common_types.h"
#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

using namespace mozilla;

namespace test {
class RtpSourcesTest : public ::testing::Test {
  using RtpSourceHistory = RtpSourceObserver::RtpSourceHistory;
  using RtpSourceEntry = mozilla::RtpSourceObserver::RtpSourceEntry;

 public:
  // History Tests

  // Test init happens as expected
  void TestInitState() {
    RtpSourceHistory history;
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(0));
    const auto& e = history.mLatestEviction;
    EXPECT_FALSE(history.mHasEvictedEntry);
    EXPECT_EQ(e.predictedPlayoutTime, 0);
    EXPECT_FALSE(e.hasAudioLevel);
    EXPECT_EQ(e.audioLevel, 0);
  }

  // Test adding into the jitter window
  void TestInsertIntoJitterWindow() {
    const bool hasAudioLevel = true;
    const uint8_t audioLevel = 10;
    const int64_t jitter = 10;
    RtpSourceHistory history;
    const int64_t times[] = {100, 120, 140, 160, 180, 200, 220};
    const size_t numEntries = sizeof(times) / sizeof(times[0]);
    for (auto i : times) {
      history.Insert(i, i + jitter, i, hasAudioLevel, audioLevel);
    }
    ASSERT_EQ(history.mDetailedHistory.size(), numEntries);
    for (auto i : times) {
      auto entry = history.FindClosestNotAfter(i + jitter);
      ASSERT_NE(entry, nullptr);
      if (entry) {
        EXPECT_EQ(entry->predictedPlayoutTime, i + jitter);
        EXPECT_EQ(entry->hasAudioLevel, hasAudioLevel);
        EXPECT_EQ(entry->audioLevel, audioLevel);
      }
    }
  }

  // Tests inserting before the jitter window, in long term history.
  void TestInsertIntoLongTerm() {
    RtpSourceHistory history;

    constexpr int64_t timeNow = 100000;
    // Should be discarded as too old
    constexpr int64_t time0 = timeNow - (10 * 1000) - 1;
    // Shold be commited
    constexpr int64_t time1 = timeNow - (10 * 1000);
    // Should be commited because it is newer
    constexpr int64_t time2 = timeNow - (5 * 1000);
    // Should be discarded because it is older
    constexpr int64_t time3 = timeNow - (7 * 1000);
    // Prune time0 should not prune time2
    constexpr int64_t pruneTime0 = time2 + (10 * 1000);
    // Prune time1 should prune time2
    constexpr int64_t pruneTime1 = time2 + (10 * 1000) + 1;

    // time0
    history.Insert(timeNow, time0, 0, true, 0);
    EXPECT_TRUE(history.Empty());
    EXPECT_FALSE(history.mHasEvictedEntry);

    // time1
    history.Insert(timeNow, time1, 1, true, 0);
    // Check that the jitter window buffer hasn't been used
    EXPECT_TRUE(history.Empty());
    ASSERT_EQ(history.mLatestEviction.predictedPlayoutTime, time1);
    EXPECT_TRUE(history.mHasEvictedEntry);

    // time2
    history.Insert(timeNow, time2, 2, true, 0);
    EXPECT_TRUE(history.Empty());
    ASSERT_EQ(history.mLatestEviction.predictedPlayoutTime, time2);
    EXPECT_TRUE(history.mHasEvictedEntry);

    // time3
    history.Insert(timeNow, time3, 3, true, 0);
    EXPECT_TRUE(history.Empty());
    ASSERT_EQ(history.mLatestEviction.predictedPlayoutTime, time2);
    EXPECT_TRUE(history.mHasEvictedEntry);

    // pruneTime0
    history.Prune(pruneTime0);
    EXPECT_TRUE(history.Empty());
    ASSERT_EQ(history.mLatestEviction.predictedPlayoutTime, time2);
    EXPECT_TRUE(history.mHasEvictedEntry);

    // pruneTime1
    history.Prune(pruneTime1);
    EXPECT_TRUE(history.Empty());
    EXPECT_FALSE(history.mHasEvictedEntry);
  }

  // Tests that a value inserted into the jitter window will age into long term
  // storage
  void TestAgeIntoLongTerm() {
    RtpSourceHistory history;
    constexpr int64_t jitterWindow = RtpSourceHistory::kMinJitterWindow;
    constexpr int64_t jitter = jitterWindow / 2;
    constexpr int64_t timeNow0 = 100000;
    constexpr int64_t time0 = timeNow0;
    constexpr int64_t time1 = timeNow0 + jitter;
    // Prune at timeNow1 should evict time0
    constexpr int64_t timeNow1 = time0 + jitterWindow + 1;
    // Prune at timeNow2 should evict time1
    constexpr int64_t timeNow2 = time1 + jitterWindow + 1;

    // time0
    history.Insert(timeNow0, time0, 0, false, 1);
    EXPECT_FALSE(history.Empty());
    // Jitter window should not have grown
    ASSERT_EQ(history.mMaxJitterWindow, jitterWindow);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(1));
    EXPECT_FALSE(history.mHasEvictedEntry);

    // time1
    history.Insert(timeNow0, time1, 1, true, 2);
    ASSERT_EQ(history.mMaxJitterWindow, jitterWindow);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(2));
    EXPECT_FALSE(history.mHasEvictedEntry);

    // Prune at timeNow1
    history.Prune(timeNow1);
    ASSERT_EQ(history.mMaxJitterWindow, jitterWindow);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(1));
    EXPECT_TRUE(history.mHasEvictedEntry);
    ASSERT_EQ(history.mLatestEviction.predictedPlayoutTime, time0);
    ASSERT_EQ(history.mLatestEviction.hasAudioLevel, false);
    ASSERT_EQ(history.mLatestEviction.audioLevel, 1);

    // Prune at timeNow1
    history.Prune(timeNow2);
    EXPECT_EQ(history.mMaxJitterWindow, jitterWindow);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(0));
    EXPECT_TRUE(history.mHasEvictedEntry);
    EXPECT_EQ(history.mLatestEviction.predictedPlayoutTime, time1);
    EXPECT_EQ(history.mLatestEviction.hasAudioLevel, true);
    EXPECT_EQ(history.mLatestEviction.audioLevel, 2);
  }

  // Packets have a maximum audio level of 127
  void TestMaximumAudioLevel() {
    RtpSourceHistory history;
    constexpr int64_t timeNow = 0;
    const int64_t jitterAdjusted = timeNow + 10;
    const uint32_t ntpTimestamp = 0;
    const bool hasAudioLevel = true;
    const uint8_t audioLevel0 = 127;
    // should result in in hasAudioLevel = false
    const uint8_t audioLevel1 = 255;
    // should result in in hasAudioLevel = false
    const uint8_t audioLevel2 = 128;

    // audio level 0
    history.Insert(timeNow, jitterAdjusted, ntpTimestamp, hasAudioLevel,
                   audioLevel0);
    ASSERT_FALSE(history.mHasEvictedEntry);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(1));
    {
      auto* entry = history.FindClosestNotAfter(jitterAdjusted);
      ASSERT_NE(entry, nullptr);
      EXPECT_TRUE(entry->hasAudioLevel);
      EXPECT_EQ(entry->audioLevel, audioLevel0);
    }
    // audio level 1
    history.Insert(timeNow, jitterAdjusted, ntpTimestamp, hasAudioLevel,
                   audioLevel1);
    ASSERT_FALSE(history.mHasEvictedEntry);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(1));
    {
      auto* entry = history.FindClosestNotAfter(jitterAdjusted);
      ASSERT_NE(entry, nullptr);
      EXPECT_FALSE(entry->hasAudioLevel);
      EXPECT_EQ(entry->audioLevel, audioLevel1);
    }
    // audio level 2
    history.Insert(timeNow, jitterAdjusted, ntpTimestamp, hasAudioLevel,
                   audioLevel2);
    ASSERT_FALSE(history.mHasEvictedEntry);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(1));
    {
      auto* entry = history.FindClosestNotAfter(jitterAdjusted);
      ASSERT_NE(entry, nullptr);
      EXPECT_FALSE(entry->hasAudioLevel);
      EXPECT_EQ(entry->audioLevel, audioLevel2);
    }
  }

  void TestEmptyPrune() {
    // Empty Prune
    RtpSourceHistory history;
    const int64_t timeNow = 0;
    history.Prune(timeNow);
    EXPECT_TRUE(history.Empty());
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(0));
    EXPECT_FALSE(history.mHasEvictedEntry);
  }

  void TestSinglePrune() {
    RtpSourceHistory history;
    constexpr int64_t timeNow = 10000;
    constexpr int64_t jitter = RtpSourceHistory::kMinJitterWindow / 2;
    const int64_t jitterAdjusted = timeNow + jitter;
    const uint32_t ntpTimestamp = 0;

    history.Insert(timeNow, jitterAdjusted, ntpTimestamp, false, 0);
    history.Prune(timeNow + (jitter * 3) + 1);
    EXPECT_EQ(history.mDetailedHistory.size(), static_cast<size_t>(0));
    EXPECT_TRUE(history.mHasEvictedEntry);
    EXPECT_EQ(jitterAdjusted, history.mLatestEviction.predictedPlayoutTime);
  }

  // Observer tests that keys are properly handled
  void TestKeyManipulation() {
    constexpr unsigned int ssrc = 4682;
    constexpr unsigned int csrc = 4682;
    auto ssrcKey = RtpSourceObserver::GetKey(
        ssrc, dom::RTCRtpSourceEntryType::Synchronization);
    auto csrcKey = RtpSourceObserver::GetKey(
        csrc, dom::RTCRtpSourceEntryType::Contributing);
    // SSRC and CSRC keys are not the same
    EXPECT_NE(ssrcKey, csrcKey);
    // Keys can be reversed back to source
    EXPECT_EQ(RtpSourceObserver::GetSourceFromKey(ssrcKey), ssrc);
    EXPECT_EQ(RtpSourceObserver::GetSourceFromKey(csrcKey), csrc);
    // Keys can be reversed back to type
    EXPECT_EQ(RtpSourceObserver::GetTypeFromKey(ssrcKey),
              dom::RTCRtpSourceEntryType::Synchronization);
    EXPECT_EQ(RtpSourceObserver::GetTypeFromKey(csrcKey),
              dom::RTCRtpSourceEntryType::Contributing);
  }

  // Observer a header with a single Csrc
  void TestObserveOneCsrc() {
    RefPtr<RtpSourceObserver> observer =
        new RtpSourceObserver(dom::RTCStatsTimestampMaker());
    webrtc::RTPHeader header;
    constexpr unsigned int ssrc = 857265;
    constexpr unsigned int csrc = 3268365;
    constexpr int64_t jitter = 0;

    header.ssrc = ssrc;
    header.numCSRCs = 1;
    header.arrOfCSRCs[0] = csrc;
    observer->OnRtpPacket(header, jitter);

    // One for the SSRC, one for the CSRC
    EXPECT_EQ(observer->mRtpSources.size(), static_cast<size_t>(2));
    nsTArray<dom::RTCRtpSourceEntry> outLevels;
    observer->GetRtpSources(outLevels);
    EXPECT_EQ(outLevels.Length(), static_cast<size_t>(2));
    bool ssrcFound = false;
    bool csrcFound = true;
    for (auto& entry : outLevels) {
      if (entry.mSource == ssrc) {
        ssrcFound = true;
        EXPECT_EQ(entry.mSourceType,
                  dom::RTCRtpSourceEntryType::Synchronization);
      }
      if (entry.mSource == csrc) {
        csrcFound = true;
        EXPECT_EQ(entry.mSourceType, dom::RTCRtpSourceEntryType::Contributing);
      }
    }
    EXPECT_TRUE(ssrcFound);
    EXPECT_TRUE(csrcFound);
  }

  // Observer a header with two CSRCs
  void TestObserveTwoCsrcs() {
    RefPtr<RtpSourceObserver> observer =
        new RtpSourceObserver(dom::RTCStatsTimestampMaker());
    webrtc::RTPHeader header;
    constexpr unsigned int ssrc = 239485;
    constexpr unsigned int csrc0 = 3425;
    constexpr unsigned int csrc1 = 36457;
    constexpr int64_t jitter = 0;

    header.ssrc = ssrc;
    header.numCSRCs = 2;
    header.arrOfCSRCs[0] = csrc0;
    header.arrOfCSRCs[1] = csrc1;
    observer->OnRtpPacket(header, jitter);

    // One for the SSRC, two for the CSRCs
    EXPECT_EQ(observer->mRtpSources.size(), static_cast<size_t>(3));
    nsTArray<dom::RTCRtpSourceEntry> outLevels;
    observer->GetRtpSources(outLevels);
    EXPECT_EQ(outLevels.Length(), static_cast<size_t>(3));
    bool ssrcFound = false;
    bool csrc0Found = true;
    bool csrc1Found = true;
    for (auto& entry : outLevels) {
      if (entry.mSource == ssrc) {
        ssrcFound = true;
        EXPECT_EQ(entry.mSourceType,
                  dom::RTCRtpSourceEntryType::Synchronization);
      }
      if (entry.mSource == csrc0) {
        csrc0Found = true;
        EXPECT_EQ(entry.mSourceType, dom::RTCRtpSourceEntryType::Contributing);
      }
      if (entry.mSource == csrc1) {
        csrc1Found = true;
        EXPECT_EQ(entry.mSourceType, dom::RTCRtpSourceEntryType::Contributing);
      }
    }
    EXPECT_TRUE(ssrcFound);
    EXPECT_TRUE(csrc0Found);
    EXPECT_TRUE(csrc1Found);
  }

  // Observer a header with a CSRC with audio level extension
  void TestObserveCsrcWithAudioLevel() {
    RefPtr<RtpSourceObserver> observer =
        new RtpSourceObserver(dom::RTCStatsTimestampMaker());
    webrtc::RTPHeader header;
  }
};

TEST_F(RtpSourcesTest, TestInitState) { TestInitState(); }
TEST_F(RtpSourcesTest, TestInsertIntoJitterWindow) {
  TestInsertIntoJitterWindow();
}
TEST_F(RtpSourcesTest, TestAgeIntoLongTerm) { TestAgeIntoLongTerm(); }
TEST_F(RtpSourcesTest, TestInsertIntoLongTerm) { TestInsertIntoLongTerm(); }
TEST_F(RtpSourcesTest, TestMaximumAudioLevel) { TestMaximumAudioLevel(); }
TEST_F(RtpSourcesTest, TestEmptyPrune) { TestEmptyPrune(); }
TEST_F(RtpSourcesTest, TestSinglePrune) { TestSinglePrune(); }
TEST_F(RtpSourcesTest, TestKeyManipulation) { TestKeyManipulation(); }
TEST_F(RtpSourcesTest, TestObserveOneCsrc) { TestObserveOneCsrc(); }
TEST_F(RtpSourcesTest, TestObserveTwoCsrcs) { TestObserveTwoCsrcs(); }
TEST_F(RtpSourcesTest, TestObserveCsrcWithAudioLevel) {
  TestObserveCsrcWithAudioLevel();
}
}  // namespace test
