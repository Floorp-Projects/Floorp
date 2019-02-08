/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PageInformation.h"

#include "ProfileJSONWriter.h"

PageInformation::PageInformation(const nsID& aDocShellId,
                                 uint32_t aDocShellHistoryId,
                                 const nsCString& aUrl, bool aIsSubFrame)
    : mDocShellId(aDocShellId),
      mDocShellHistoryId(aDocShellHistoryId),
      mUrl(aUrl),
      mIsSubFrame(aIsSubFrame) {}

bool PageInformation::Equals(PageInformation* aOtherPageInfo) {
  return DocShellHistoryId() == aOtherPageInfo->DocShellHistoryId() &&
         DocShellId().Equals(aOtherPageInfo->DocShellId()) &&
         IsSubFrame() == aOtherPageInfo->IsSubFrame();
}

void PageInformation::StreamJSON(SpliceableJSONWriter& aWriter) {
  aWriter.StartObjectElement();
  aWriter.StringProperty("docshellId", nsIDToCString(DocShellId()).get());
  aWriter.DoubleProperty("historyId", DocShellHistoryId());
  aWriter.StringProperty("url", Url().get());
  aWriter.BoolProperty("isSubFrame", IsSubFrame());
  aWriter.EndObject();
}

size_t PageInformation::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}
