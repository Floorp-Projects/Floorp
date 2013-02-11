/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/vp8_partition_aggregator.h"

#include <assert.h>
#include <stdlib.h>  // NULL

#include <algorithm>
#include <limits>

namespace webrtc {

PartitionTreeNode::PartitionTreeNode(PartitionTreeNode* parent,
                                     const int* size_vector,
                                     int num_partitions,
                                     int this_size)
    : parent_(parent),
      this_size_(this_size),
      size_vector_(size_vector),
      num_partitions_(num_partitions),
      max_parent_size_(0),
      min_parent_size_(std::numeric_limits<int>::max()),
      packet_start_(false) {
  assert(num_partitions >= 0);
  children_[kLeftChild] = NULL;
  children_[kRightChild] = NULL;
}

PartitionTreeNode* PartitionTreeNode::CreateRootNode(const int* size_vector,
                                                     int num_partitions) {
  PartitionTreeNode* root_node =
      new PartitionTreeNode(NULL, &size_vector[1], num_partitions - 1,
                            size_vector[0]);
  root_node->set_packet_start(true);
  return root_node;
}

PartitionTreeNode::~PartitionTreeNode() {
  delete children_[kLeftChild];
  delete children_[kRightChild];
}

int PartitionTreeNode::Cost(int penalty) {
  assert(penalty >= 0);
  int cost = 0;
  if (num_partitions_ == 0) {
    // This is a solution node.
    cost = std::max(max_parent_size_, this_size_) -
        std::min(min_parent_size_, this_size_);
  } else {
    cost = std::max(max_parent_size_, this_size_) - min_parent_size_;
  }
  return cost + NumPackets() * penalty;
}

bool PartitionTreeNode::CreateChildren(int max_size) {
  assert(max_size > 0);
  bool children_created = false;
  if (num_partitions_ > 0) {
    if (this_size_ + size_vector_[0] <= max_size) {
      assert(!children_[kLeftChild]);
      children_[kLeftChild] =
          new PartitionTreeNode(this,
                                &size_vector_[1],
                                num_partitions_ - 1,
                                this_size_ + size_vector_[0]);
      children_[kLeftChild]->set_max_parent_size(max_parent_size_);
      children_[kLeftChild]->set_min_parent_size(min_parent_size_);
      // "Left" child is continuation of same packet.
      children_[kLeftChild]->set_packet_start(false);
      children_created = true;
    }
    if (this_size_ > 0) {
      assert(!children_[kRightChild]);
      children_[kRightChild] = new PartitionTreeNode(this,
                                                     &size_vector_[1],
                                                     num_partitions_ - 1,
                                                     size_vector_[0]);
      children_[kRightChild]->set_max_parent_size(
          std::max(max_parent_size_, this_size_));
      children_[kRightChild]->set_min_parent_size(
          std::min(min_parent_size_, this_size_));
      // "Right" child starts a new packet.
      children_[kRightChild]->set_packet_start(true);
      children_created = true;
    }
  }
  return children_created;
}

int PartitionTreeNode::NumPackets() {
  if (parent_ == NULL) {
    // Root node is a "right" child by definition.
    return 1;
  }
  if (parent_->children_[kLeftChild] == this) {
    // This is a "left" child.
    return parent_->NumPackets();
  } else {
    // This is a "right" child.
    return 1 + parent_->NumPackets();
  }
}

PartitionTreeNode* PartitionTreeNode::GetOptimalNode(int max_size,
                                                     int penalty) {
  CreateChildren(max_size);
  PartitionTreeNode* left = children_[kLeftChild];
  PartitionTreeNode* right = children_[kRightChild];
  if ((left == NULL) && (right == NULL)) {
    // This is a solution node; return it.
    return this;
  } else if (left == NULL) {
    // One child empty, return the other.
    return right->GetOptimalNode(max_size, penalty);
  } else if (right == NULL) {
    // One child empty, return the other.
    return left->GetOptimalNode(max_size, penalty);
  } else {
    PartitionTreeNode* first;
    PartitionTreeNode* second;
    if (left->Cost(penalty) <= right->Cost(penalty)) {
      first = left;
      second = right;
    } else {
      first = right;
      second = left;
    }
    first = first->GetOptimalNode(max_size, penalty);
    if (second->Cost(penalty) <= first->Cost(penalty)) {
      second = second->GetOptimalNode(max_size, penalty);
      // Compare cost estimate for "second" with actual cost for "first".
      if (second->Cost(penalty) < first->Cost(penalty)) {
        return second;
      }
    }
    return first;
  }
}

Vp8PartitionAggregator::Vp8PartitionAggregator(
    const RTPFragmentationHeader& fragmentation,
    int first_partition_idx, int last_partition_idx)
    : root_(NULL),
      num_partitions_(last_partition_idx - first_partition_idx + 1),
      size_vector_(new int[num_partitions_]),
      largest_partition_size_(0) {
  assert(first_partition_idx >= 0);
  assert(last_partition_idx >= first_partition_idx);
  assert(last_partition_idx < fragmentation.fragmentationVectorSize);
  for (size_t i = 0; i < num_partitions_; ++i) {
    size_vector_[i] =
        fragmentation.fragmentationLength[i + first_partition_idx];
    largest_partition_size_ = std::max(largest_partition_size_,
                                       size_vector_[i]);
  }
  root_ = PartitionTreeNode::CreateRootNode(size_vector_, num_partitions_);
}

Vp8PartitionAggregator::~Vp8PartitionAggregator() {
  delete [] size_vector_;
  delete root_;
}

void Vp8PartitionAggregator::SetPriorMinMax(int min_size, int max_size) {
  assert(root_);
  assert(min_size >= 0);
  assert(max_size >= min_size);
  root_->set_min_parent_size(min_size);
  root_->set_max_parent_size(max_size);
}

Vp8PartitionAggregator::ConfigVec
Vp8PartitionAggregator::FindOptimalConfiguration(int max_size, int penalty) {
  assert(root_);
  assert(max_size >= largest_partition_size_);
  PartitionTreeNode* opt = root_->GetOptimalNode(max_size, penalty);
  ConfigVec config_vector(num_partitions_, 0);
  PartitionTreeNode* temp_node = opt;
  int packet_index = opt->NumPackets() - 1;
  for (int i = num_partitions_ - 1; i >= 0; --i) {
    assert(packet_index >= 0);
    assert(temp_node != NULL);
    config_vector[i] = packet_index;
    if (temp_node->packet_start()) --packet_index;
    temp_node = temp_node->parent();
  }
  return config_vector;
}

void Vp8PartitionAggregator::CalcMinMax(const ConfigVec& config,
                                        int* min_size, int* max_size) const {
  if (*min_size < 0) {
    *min_size = std::numeric_limits<int>::max();
  }
  if (*max_size < 0) {
    *max_size = 0;
  }
  unsigned int i = 0;
  while (i < config.size()) {
    int this_size = 0;
    unsigned int j = i;
    while (j < config.size() && config[i] == config[j]) {
      this_size += size_vector_[j];
      ++j;
    }
    i = j;
    if (this_size < *min_size) {
      *min_size = this_size;
    }
    if (this_size > *max_size) {
      *max_size = this_size;
    }
  }
}

int Vp8PartitionAggregator::CalcNumberOfFragments(int large_partition_size,
                                                  int max_payload_size,
                                                  int penalty,
                                                  int min_size,
                                                  int max_size) {
  assert(max_size <= max_payload_size);
  assert(min_size <= max_size);
  assert(max_payload_size > 0);
  // Divisions with rounding up.
  const int min_number_of_fragments =
      (large_partition_size + max_payload_size - 1) / max_payload_size;
  if (min_size < 0 || max_size < 0) {
    // No aggregates produced, so we do not have any size boundaries.
    // Simply split in as few partitions as possible.
    return min_number_of_fragments;
  }
  const int max_number_of_fragments =
      (large_partition_size + min_size - 1) / min_size;
  int num_fragments = -1;
  int best_cost = std::numeric_limits<int>::max();
  for (int n = min_number_of_fragments; n <= max_number_of_fragments; ++n) {
    // Round up so that we use the largest fragment.
    int fragment_size = (large_partition_size + n - 1) / n;
    int cost = 0;
    if (fragment_size < min_size) {
      cost = min_size - fragment_size + n * penalty;
    } else if (fragment_size > max_size) {
      cost = fragment_size - max_size + n * penalty;
    } else {
      cost = n * penalty;
    }
    if (fragment_size <= max_payload_size && cost < best_cost) {
      num_fragments = n;
      best_cost = cost;
    }
  }
  assert(num_fragments > 0);
  // TODO(mflodman) Assert disabled since it's falsely triggered, see issue 293.
  //assert(large_partition_size / num_fragments + 1 <= max_payload_size);
  return num_fragments;
}

}  // namespace

