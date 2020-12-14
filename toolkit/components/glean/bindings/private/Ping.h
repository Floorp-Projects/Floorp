/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_Ping_h
#define mozilla_glean_Ping_h

#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/Maybe.h"
#include "nsIGleanMetrics.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

class Ping {
 public:
  constexpr explicit Ping(uint32_t aId) : mId(aId) {}

  /**
   * Collect and submit the ping for eventual upload.
   *
   * This will collect all stored data to be included in the ping.
   * Data with lifetime `ping` will then be reset.
   *
   * If the ping is configured with `send_if_empty = false`
   * and the ping currently contains no content,
   * it will not be queued for upload.
   * If the ping is configured with `send_if_empty = true`
   * it will be queued for upload even if empty.
   *
   * Pings always contain the `ping_info` and `client_info` sections.
   * See [ping
   * sections](https://mozilla.github.io/glean/book/user/pings/index.html#ping-sections)
   * for details.
   *
   * @param aReason - Optional. The reason the ping is being submitted.
   *                  Must match one of the configured `reason_codes`.
   */
  void Submit(const nsACString& aReason) const {
    fog_submit_ping_by_id(mId, &aReason);
  }

 private:
  const uint32_t mId;
};

}  // namespace impl

class GleanPing final : public nsIGleanPing {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANPING

  explicit GleanPing(uint32_t aId) : mPing(aId) {}

 private:
  virtual ~GleanPing() = default;

  const impl::Ping mPing;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_Ping_h */
