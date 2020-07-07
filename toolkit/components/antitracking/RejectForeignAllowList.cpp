/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RejectForeignAllowList.h"
#include "mozilla/dom/Document.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsNetUtil.h"

#define REJECTFOREIGNALLOWLIST_PREF "privacy.rejectForeign.allowList"_ns
#define REJECTFOREIGNALLOWLIST_NAME "RejectForeignAllowList"_ns

namespace mozilla {

namespace {

StaticRefPtr<RejectForeignAllowList> gRejectForeignAllowList;

}  // namespace

// static
bool RejectForeignAllowList::Check(dom::Document* aDocument) {
  MOZ_ASSERT(aDocument);

  nsIURI* documentURI = aDocument->GetDocumentURI();
  if (!documentURI) {
    return false;
  }

  return GetOrCreate()->CheckInternal(documentURI);
}

// static
bool RejectForeignAllowList::Check(nsIHttpChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIURI> channelURI;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return GetOrCreate()->CheckInternal(channelURI);
}

// static
RejectForeignAllowList* RejectForeignAllowList::GetOrCreate() {
  if (!gRejectForeignAllowList) {
    gRejectForeignAllowList = new RejectForeignAllowList();

    nsCOMPtr<nsIUrlClassifierExceptionListService> exceptionListService =
        do_GetService("@mozilla.org/url-classifier/exception-list-service;1");
    if (exceptionListService) {
      exceptionListService->RegisterAndRunExceptionListObserver(
          REJECTFOREIGNALLOWLIST_NAME, REJECTFOREIGNALLOWLIST_PREF,
          gRejectForeignAllowList);
    }

    RunOnShutdown([exceptionListService] {
      if (gRejectForeignAllowList) {
        if (exceptionListService) {
          exceptionListService->UnregisterExceptionListObserver(
              REJECTFOREIGNALLOWLIST_NAME, gRejectForeignAllowList);
        }
        gRejectForeignAllowList = nullptr;
      }
    });
  }

  return gRejectForeignAllowList;
}

bool RejectForeignAllowList::CheckInternal(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return nsContentUtils::IsURIInList(aURI, mList);
}

NS_IMETHODIMP
RejectForeignAllowList::OnExceptionListUpdate(const nsACString& aList) {
  mList = aList;
  return NS_OK;
}

RejectForeignAllowList::RejectForeignAllowList() = default;
RejectForeignAllowList::~RejectForeignAllowList() = default;

NS_INTERFACE_MAP_BEGIN(RejectForeignAllowList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports,
                                   nsIUrlClassifierExceptionListObserver)
  NS_INTERFACE_MAP_ENTRY(nsIUrlClassifierExceptionListObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(RejectForeignAllowList)
NS_IMPL_RELEASE(RejectForeignAllowList)

}  // namespace mozilla
