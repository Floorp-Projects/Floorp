/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>  // NULL

#include "gtest/gtest.h"
#include "modules/rtp_rtcp/source/vp8_partition_aggregator.h"

namespace webrtc {

TEST(PartitionTreeNode, CreateAndDelete) {
  const int kVector[] = {1, 2, 3};
  const int kNumPartitions = sizeof(kVector) / sizeof(kVector[0]);
  PartitionTreeNode* node1 =
      PartitionTreeNode::CreateRootNode(kVector, kNumPartitions);
  PartitionTreeNode* node2 =
      new PartitionTreeNode(node1, kVector, kNumPartitions, 17);
  delete node1;
  delete node2;
}

TEST(PartitionTreeNode, CreateChildrenAndDelete) {
  const int kVector[] = {1, 2, 3};
  const int kNumPartitions = sizeof(kVector) / sizeof(kVector[0]);
  const int kMaxSize = 10;
  const int kPenalty = 5;
  PartitionTreeNode* root =
      PartitionTreeNode::CreateRootNode(kVector, kNumPartitions);
  EXPECT_TRUE(root->CreateChildren(kMaxSize));
  ASSERT_TRUE(NULL != root->left_child());
  ASSERT_TRUE(NULL != root->right_child());
  EXPECT_EQ(3, root->left_child()->this_size());
  EXPECT_EQ(2, root->right_child()->this_size());
  EXPECT_EQ(11, root->right_child()->Cost(kPenalty));
  EXPECT_FALSE(root->left_child()->packet_start());
  EXPECT_TRUE(root->right_child()->packet_start());
  delete root;
}

TEST(PartitionTreeNode, FindOptimalConfig) {
  const int kVector[] = {197, 194, 213, 215, 184, 199, 197, 207};
  const int kNumPartitions = sizeof(kVector) / sizeof(kVector[0]);
  const int kMaxSize = 1500;
  const int kPenalty = 1;
  PartitionTreeNode* root =
      PartitionTreeNode::CreateRootNode(kVector, kNumPartitions);
  root->set_max_parent_size(500);
  root->set_min_parent_size(300);
  PartitionTreeNode* opt = root->GetOptimalNode(kMaxSize, kPenalty);
  ASSERT_TRUE(opt != NULL);
  EXPECT_EQ(4, opt->NumPackets());
  // Expect optimal sequence to be {1, 0, 1, 0, 1, 0, 1, 0}, which corresponds
  // to (right)-left-right-left-right-left-right-left, where the root node is
  // implicitly a "right" node by definition.
  EXPECT_TRUE(opt->parent()->parent()->parent()->parent()->parent()->
              parent()->parent()->packet_start());
  EXPECT_FALSE(opt->parent()->parent()->parent()->parent()->parent()->
               parent()->packet_start());
  EXPECT_TRUE(opt->parent()->parent()->parent()->parent()->parent()->
              packet_start());
  EXPECT_FALSE(opt->parent()->parent()->parent()->parent()->packet_start());
  EXPECT_TRUE(opt->parent()->parent()->parent()->packet_start());
  EXPECT_FALSE(opt->parent()->parent()->packet_start());
  EXPECT_TRUE(opt->parent()->packet_start());
  EXPECT_FALSE(opt->packet_start());
  EXPECT_TRUE(opt == root->left_child()->right_child()->left_child()->
              right_child()->left_child()->right_child()->left_child());
  delete root;
}

TEST(PartitionTreeNode, FindOptimalConfigSinglePartition) {
  const int kVector[] = {17};
  const int kNumPartitions = sizeof(kVector) / sizeof(kVector[0]);
  const int kMaxSize = 1500;
  const int kPenalty = 1;
  PartitionTreeNode* root =
      PartitionTreeNode::CreateRootNode(kVector, kNumPartitions);
  PartitionTreeNode* opt = root->GetOptimalNode(kMaxSize, kPenalty);
  ASSERT_TRUE(opt != NULL);
  EXPECT_EQ(1, opt->NumPackets());
  EXPECT_TRUE(opt == root);
  delete root;
}

static void VerifyConfiguration(const int* expected_config,
                                size_t expected_config_len,
                                const std::vector<int>& opt_config,
                                const RTPFragmentationHeader& fragmentation) {
  ASSERT_EQ(expected_config_len, fragmentation.fragmentationVectorSize);
  EXPECT_EQ(expected_config_len, opt_config.size());
  for (size_t i = 0; i < expected_config_len; ++i) {
    EXPECT_EQ(expected_config[i], opt_config[i]);
  }
}

static void VerifyMinMax(const Vp8PartitionAggregator& aggregator,
                         const std::vector<int>& opt_config,
                         int expected_min,
                         int expected_max) {
  int min_size = -1;
  int max_size = -1;
  aggregator.CalcMinMax(opt_config, &min_size, &max_size);
  EXPECT_EQ(expected_min, min_size);
  EXPECT_EQ(expected_max, max_size);
}

TEST(Vp8PartitionAggregator, CreateAndDelete) {
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  Vp8PartitionAggregator* aggregator =
      new Vp8PartitionAggregator(fragmentation, 0, 2);
  delete aggregator;
}

TEST(Vp8PartitionAggregator, FindOptimalConfig) {
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(8);
  fragmentation.fragmentationLength[0] = 197;
  fragmentation.fragmentationLength[1] = 194;
  fragmentation.fragmentationLength[2] = 213;
  fragmentation.fragmentationLength[3] = 215;
  fragmentation.fragmentationLength[4] = 184;
  fragmentation.fragmentationLength[5] = 199;
  fragmentation.fragmentationLength[6] = 197;
  fragmentation.fragmentationLength[7] = 207;
  Vp8PartitionAggregator* aggregator =
      new Vp8PartitionAggregator(fragmentation, 0, 7);
  aggregator->SetPriorMinMax(300, 500);
  int kMaxSize = 1500;
  int kPenalty = 1;
  std::vector<int> opt_config = aggregator->FindOptimalConfiguration(kMaxSize,
                                                                     kPenalty);
  const int kExpectedConfig[] = {0, 0, 1, 1, 2, 2, 3, 3};
  const size_t kExpectedConfigSize =
      sizeof(kExpectedConfig) / sizeof(kExpectedConfig[0]);
  VerifyConfiguration(kExpectedConfig, kExpectedConfigSize, opt_config,
                      fragmentation);
  VerifyMinMax(*aggregator, opt_config, 383, 428);
  // Change min and max and run method again. This time, we expect it to leave
  // the values unchanged.
  int min_size = 382;
  int max_size = 429;
  aggregator->CalcMinMax(opt_config, &min_size, &max_size);
  EXPECT_EQ(382, min_size);
  EXPECT_EQ(429, max_size);
  delete aggregator;
}

TEST(Vp8PartitionAggregator, FindOptimalConfigEqualFragments) {
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(8);
  fragmentation.fragmentationLength[0] = 200;
  fragmentation.fragmentationLength[1] = 200;
  fragmentation.fragmentationLength[2] = 200;
  fragmentation.fragmentationLength[3] = 200;
  fragmentation.fragmentationLength[4] = 200;
  fragmentation.fragmentationLength[5] = 200;
  fragmentation.fragmentationLength[6] = 200;
  fragmentation.fragmentationLength[7] = 200;
  Vp8PartitionAggregator* aggregator =
      new Vp8PartitionAggregator(fragmentation, 0, 7);
  int kMaxSize = 1500;
  int kPenalty = 1;
  std::vector<int> opt_config = aggregator->FindOptimalConfiguration(kMaxSize,
                                                                     kPenalty);
  const int kExpectedConfig[] = {0, 0, 0, 0, 1, 1, 1, 1};
  const size_t kExpectedConfigSize =
      sizeof(kExpectedConfig) / sizeof(kExpectedConfig[0]);
  VerifyConfiguration(kExpectedConfig, kExpectedConfigSize, opt_config,
                      fragmentation);
  VerifyMinMax(*aggregator, opt_config, 800, 800);
  delete aggregator;
}

TEST(Vp8PartitionAggregator, FindOptimalConfigSinglePartition) {
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationLength[0] = 17;
  Vp8PartitionAggregator* aggregator =
      new Vp8PartitionAggregator(fragmentation, 0, 0);
  int kMaxSize = 1500;
  int kPenalty = 1;
  std::vector<int> opt_config = aggregator->FindOptimalConfiguration(kMaxSize,
                                                                     kPenalty);
  const int kExpectedConfig[] = {0};
  const size_t kExpectedConfigSize =
      sizeof(kExpectedConfig) / sizeof(kExpectedConfig[0]);
  VerifyConfiguration(kExpectedConfig, kExpectedConfigSize, opt_config,
                      fragmentation);
  VerifyMinMax(*aggregator, opt_config, 17, 17);
  delete aggregator;
}

TEST(Vp8PartitionAggregator, TestCalcNumberOfFragments) {
  const int kMTU = 1500;
  EXPECT_EQ(2,
            Vp8PartitionAggregator::CalcNumberOfFragments(
                1600, kMTU, 1, 300, 900));
  EXPECT_EQ(3,
            Vp8PartitionAggregator::CalcNumberOfFragments(
                1600, kMTU, 1, 300, 798));
  EXPECT_EQ(2,
            Vp8PartitionAggregator::CalcNumberOfFragments(
                1600, kMTU, 1, 900, 1000));
}

}  // namespace

