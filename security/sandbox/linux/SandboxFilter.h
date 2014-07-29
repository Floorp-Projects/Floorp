/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxFilter_h
#define mozilla_SandboxFilter_h

struct sock_fprog;
struct sock_filter;

namespace mozilla {

enum SandboxType {
  kSandboxContentProcess,
  kSandboxMediaPlugin
};

class SandboxFilter {
  sock_filter *mFilter;
  sock_fprog *mProg;
  const sock_fprog **mStored;
public:
  // RAII: on construction, builds the filter and stores it in the
  // provided variable (with optional logging); on destruction, frees
  // the filter and nulls out the pointer.
  SandboxFilter(const sock_fprog** aStored, SandboxType aBox,
                bool aVerbose = false);
  ~SandboxFilter();
};

} // namespace mozilla

#endif
