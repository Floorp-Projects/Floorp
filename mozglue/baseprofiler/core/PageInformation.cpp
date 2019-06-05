/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "PageInformation.h"

#  include "BaseProfileJSONWriter.h"

namespace mozilla {
namespace baseprofiler {

PageInformation::PageInformation(const std::string& aDocShellId,
                                 uint32_t aDocShellHistoryId,
                                 const std::string& aUrl, bool aIsSubFrame)
    : mDocShellId(aDocShellId),
      mDocShellHistoryId(aDocShellHistoryId),
      mUrl(aUrl),
      mIsSubFrame(aIsSubFrame),
      mRefCnt(0) {}

bool PageInformation::Equals(PageInformation* aOtherPageInfo) {
  return DocShellHistoryId() == aOtherPageInfo->DocShellHistoryId() &&
         DocShellId() == aOtherPageInfo->DocShellId() &&
         IsSubFrame() == aOtherPageInfo->IsSubFrame();
}

void PageInformation::StreamJSON(SpliceableJSONWriter& aWriter) {
  aWriter.StartObjectElement();
  aWriter.StringProperty("docshellId", DocShellId().c_str());
  aWriter.DoubleProperty("historyId", DocShellHistoryId());
  aWriter.StringProperty("url", Url().c_str());
  aWriter.BoolProperty("isSubFrame", IsSubFrame());
  aWriter.EndObject();
}

size_t PageInformation::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
