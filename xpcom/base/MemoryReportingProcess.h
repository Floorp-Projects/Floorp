/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcom_base_MemoryReportingProcess_h
#define xpcom_base_MemoryReportingProcess_h

#include <stdint.h>
#include "nscore.h"

namespace mozilla {
namespace dom {
class MaybeFileDesc;
} // namespace dom

// Top-level process actors should implement this to integrate with
// nsMemoryReportManager.
class MemoryReportingProcess
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

  virtual ~MemoryReportingProcess()
  {}

  // Return true if the process is still alive, false otherwise.
  virtual bool IsAlive() const = 0;

  // Initiate a memory report request, returning true if a report was
  // successfully initiated and false otherwise.
  virtual bool SendRequestMemoryReport(
    const uint32_t& aGeneration,
    const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage,
    const dom::MaybeFileDesc& aDMDFile) = 0;

  virtual int32_t Pid() const = 0;
};

} // namespace mozilla

#endif // xpcom_base_MemoryReportingProcess_h
