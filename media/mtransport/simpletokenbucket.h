/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

/*
 *  This file defines a dirt-simple token bucket class.
 */

#ifndef simpletokenbucket_h__
#define simpletokenbucket_h__

#include <stdint.h>

#include "prinrval.h"

#include "m_cpp_utils.h"

namespace mozilla {

class SimpleTokenBucket {
  public:
    /*
     *  Create a SimpleTokenBucket with a given maximum size and
     *  token replenishment rate.
     *  (eg; if you want a maximum rate of 5 per second over a 7 second
     *  period, call SimpleTokenBucket b(5*7, 5);)
     */
    SimpleTokenBucket(size_t bucket_size, size_t tokens_per_second);

    /*
     *  Attempt to acquire a number of tokens. If successful, returns
     *  |num_tokens|, otherwise returns the number of tokens currently
     *  in the bucket.
     *  Note: To get the number of tokens in the bucket, pass something
     *  like UINT32_MAX.
     */
    size_t getTokens(size_t num_tokens);

  protected: // Allow testing to touch these.
    uint64_t max_tokens_;
    uint64_t num_tokens_;
    size_t tokens_per_second_;
    PRIntervalTime last_time_tokens_added_;

    DISALLOW_COPY_ASSIGN(SimpleTokenBucket);
};

} // namespace mozilla

#endif // simpletokenbucket_h__

