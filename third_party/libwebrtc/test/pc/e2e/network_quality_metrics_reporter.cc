/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/network_quality_metrics_reporter.h"

#include <utility>

#include "api/stats/rtc_stats.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/metrics/metric.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;

constexpr TimeDelta kStatsWaitTimeout = TimeDelta::Seconds(1);

// Field trial which controls whether to report standard-compliant bytes
// sent/received per stream.  If enabled, padding and headers are not included
// in bytes sent or received.
constexpr char kUseStandardBytesStats[] = "WebRTC-UseStandardBytesStats";

}  // namespace

NetworkQualityMetricsReporter::NetworkQualityMetricsReporter(
    EmulatedNetworkManagerInterface* alice_network,
    EmulatedNetworkManagerInterface* bob_network,
    test::MetricsLogger* metrics_logger)
    : alice_network_(alice_network),
      bob_network_(bob_network),
      metrics_logger_(metrics_logger) {
  RTC_CHECK(metrics_logger_);
}

void NetworkQualityMetricsReporter::Start(
    absl::string_view test_case_name,
    const TrackIdStreamInfoMap* /*reporter_helper*/) {
  test_case_name_ = std::string(test_case_name);
  // Check that network stats are clean before test execution.
  std::unique_ptr<EmulatedNetworkStats> alice_stats =
      PopulateStats(alice_network_);
  RTC_CHECK_EQ(alice_stats->PacketsSent(), 0);
  RTC_CHECK_EQ(alice_stats->PacketsReceived(), 0);
  std::unique_ptr<EmulatedNetworkStats> bob_stats = PopulateStats(bob_network_);
  RTC_CHECK_EQ(bob_stats->PacketsSent(), 0);
  RTC_CHECK_EQ(bob_stats->PacketsReceived(), 0);
}

void NetworkQualityMetricsReporter::OnStatsReports(
    absl::string_view pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  DataSize payload_received = DataSize::Zero();
  DataSize payload_sent = DataSize::Zero();

  auto inbound_stats = report->GetStatsOfType<RTCInboundRTPStreamStats>();
  for (const auto& stat : inbound_stats) {
    payload_received +=
        DataSize::Bytes(stat->bytes_received.ValueOrDefault(0ul) +
                        stat->header_bytes_received.ValueOrDefault(0ul));
  }

  auto outbound_stats = report->GetStatsOfType<RTCOutboundRTPStreamStats>();
  for (const auto& stat : outbound_stats) {
    payload_sent +=
        DataSize::Bytes(stat->bytes_sent.ValueOrDefault(0ul) +
                        stat->header_bytes_sent.ValueOrDefault(0ul));
  }

  MutexLock lock(&lock_);
  PCStats& stats = pc_stats_[std::string(pc_label)];
  stats.payload_received = payload_received;
  stats.payload_sent = payload_sent;
}

void NetworkQualityMetricsReporter::StopAndReportResults() {
  std::unique_ptr<EmulatedNetworkStats> alice_stats =
      PopulateStats(alice_network_);
  std::unique_ptr<EmulatedNetworkStats> bob_stats = PopulateStats(bob_network_);
  int64_t alice_packets_loss =
      alice_stats->PacketsSent() - bob_stats->PacketsReceived();
  int64_t bob_packets_loss =
      bob_stats->PacketsSent() - alice_stats->PacketsReceived();
  ReportStats("alice", std::move(alice_stats), alice_packets_loss);
  ReportStats("bob", std::move(bob_stats), bob_packets_loss);

  if (!webrtc::field_trial::IsEnabled(kUseStandardBytesStats)) {
    RTC_LOG(LS_ERROR)
        << "Non-standard GetStats; \"payload\" counts include RTP headers";
  }

  MutexLock lock(&lock_);
  for (const auto& pair : pc_stats_) {
    ReportPCStats(pair.first, pair.second);
  }
}

std::unique_ptr<EmulatedNetworkStats>
NetworkQualityMetricsReporter::PopulateStats(
    EmulatedNetworkManagerInterface* network) {
  rtc::Event wait;
  std::unique_ptr<EmulatedNetworkStats> stats;
  network->GetStats([&](std::unique_ptr<EmulatedNetworkStats> s) {
    stats = std::move(s);
    wait.Set();
  });
  bool stats_received = wait.Wait(kStatsWaitTimeout);
  RTC_CHECK(stats_received);
  return stats;
}

void NetworkQualityMetricsReporter::ReportStats(
    const std::string& network_label,
    std::unique_ptr<EmulatedNetworkStats> stats,
    int64_t packet_loss) {
  metrics_logger_->LogSingleValueMetric(
      "bytes_sent", GetTestCaseName(network_label), stats->BytesSent().bytes(),
      Unit::kBytes, ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "packets_sent", GetTestCaseName(network_label), stats->PacketsSent(),
      Unit::kUnitless, ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "average_send_rate", GetTestCaseName(network_label),
      stats->PacketsSent() >= 2 ? stats->AverageSendRate().kbps<double>() : 0,
      Unit::kKilobitsPerSecond, ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "bytes_discarded_no_receiver", GetTestCaseName(network_label),
      stats->BytesDropped().bytes(), Unit::kBytes,
      ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "packets_discarded_no_receiver", GetTestCaseName(network_label),
      stats->PacketsDropped(), Unit::kUnitless,
      ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "bytes_received", GetTestCaseName(network_label),
      stats->BytesReceived().bytes(), Unit::kBytes,
      ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "packets_received", GetTestCaseName(network_label),
      stats->PacketsReceived(), Unit::kUnitless,
      ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "average_receive_rate", GetTestCaseName(network_label),
      stats->PacketsReceived() >= 2 ? stats->AverageReceiveRate().kbps<double>()
                                    : 0,
      Unit::kKilobitsPerSecond, ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "sent_packets_loss", GetTestCaseName(network_label), packet_loss,
      Unit::kUnitless, ImprovementDirection::kNeitherIsBetter);
}

void NetworkQualityMetricsReporter::ReportPCStats(const std::string& pc_label,
                                                  const PCStats& stats) {
  metrics_logger_->LogSingleValueMetric(
      "payload_bytes_received", pc_label, stats.payload_received.bytes(),
      Unit::kBytes, ImprovementDirection::kNeitherIsBetter);
  metrics_logger_->LogSingleValueMetric(
      "payload_bytes_sent", pc_label, stats.payload_sent.bytes(), Unit::kBytes,
      ImprovementDirection::kNeitherIsBetter);
}

std::string NetworkQualityMetricsReporter::GetTestCaseName(
    const std::string& network_label) const {
  return test_case_name_ + "/" + network_label;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
