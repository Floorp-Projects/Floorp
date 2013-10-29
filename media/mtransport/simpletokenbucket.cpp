/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

#include "simpletokenbucket.h"

#include <stdint.h>

#include "prinrval.h"

namespace mozilla {

SimpleTokenBucket::SimpleTokenBucket(size_t bucket_size,
                                     size_t tokens_per_second) :
  max_tokens_(bucket_size),
  num_tokens_(bucket_size),
  tokens_per_second_(tokens_per_second),
  last_time_tokens_added_(PR_IntervalNow()) {
}

size_t SimpleTokenBucket::getTokens(size_t num_requested_tokens) {
  // Only fill if there isn't enough to satisfy the request.
  // If we get tokens so seldomly that we are able to roll the timer all
  // the way around its range, then we lose that entire range of time
  // for token accumulation. Probably not the end of the world.
  if (num_requested_tokens > num_tokens_) {
    PRIntervalTime now = PR_IntervalNow();

    // If we roll over the max, since everything in this calculation is the same
    // unsigned type, this will still yield the elapsed time (unless we've
    // wrapped more than once).
    PRIntervalTime elapsed_ticks = now - last_time_tokens_added_;

    uint32_t elapsed_milli_sec = PR_IntervalToMilliseconds(elapsed_ticks);
    size_t tokens_to_add = (elapsed_milli_sec * tokens_per_second_)/1000;

    // Only update our timestamp if we added some tokens
    // TODO:(bcampen@mozilla.com) Should we attempt to "save" leftover time?
    if (tokens_to_add) {
      num_tokens_ += tokens_to_add;
      if (num_tokens_ > max_tokens_) {
        num_tokens_ = max_tokens_;
      }

      last_time_tokens_added_ = now;
    }

    if (num_requested_tokens > num_tokens_) {
      return num_tokens_;
    }
  }

  num_tokens_ -= num_requested_tokens;
  return num_requested_tokens;
}

} // namespace mozilla

