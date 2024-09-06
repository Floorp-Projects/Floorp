/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentAnalysis.h"
#include "mozilla/ClipboardContentAnalysisParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Variant.h"
#include "nsBaseClipboard.h"
#include "nsIClipboard.h"
#include "nsID.h"
#include "nsITransferable.h"

namespace mozilla {
namespace {
using ClipboardResultPromise =
    MozPromise<dom::IPCTransferableData, nsresult, true>;

static RefPtr<ClipboardResultPromise> GetClipboardImpl(
    const nsTArray<nsCString>& aTypes, int32_t aWhichClipboard,
    uint64_t aRequestingWindowContextId,
    dom::ThreadsafeContentParentHandle* aRequestingContentParent) {
  AssertIsOnMainThread();

  RefPtr<dom::WindowGlobalParent> window =
      dom::WindowGlobalParent::GetByInnerWindowId(aRequestingWindowContextId);

  // We expect content processes to always pass a non-null window so
  // Content Analysis can analyze it. (if Content Analysis is
  // active) There may be some cases when a window is closing, etc.,
  // in which case returning no clipboard content should not be a
  // problem.
  if (!window) {
    return ClipboardResultPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (window->IsDiscarded()) {
    NS_WARNING(
        "discarded window passed to RecvGetClipboard(); returning "
        "no clipboard "
        "content");
    return ClipboardResultPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (aRequestingContentParent->ChildID() != window->ContentParentId()) {
    NS_WARNING("incorrect content process passing window to GetClipboard");
    return ClipboardResultPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Retrieve clipboard
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1");
  if (!clipboard) {
    return ClipboardResultPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  auto transferableToCheck =
      dom::ContentParent::CreateClipboardTransferable(aTypes);
  if (transferableToCheck.isErr()) {
    return ClipboardResultPromise::CreateAndReject(
        transferableToCheck.unwrapErr(), __func__);
  }

  // Pass nullptr for the window here because we will be doing
  // content analysis ourselves asynchronously (so it doesn't block
  // main thread we're running on now)
  nsCOMPtr transferable = transferableToCheck.unwrap();
  // Ideally we would be calling GetDataSnapshot() here to avoid blocking the
  // main thread (and this would mean we could also pass in the window here so
  // we wouldn't have to duplicate the Content Analysis code below). See
  // bug 1908280.
  nsresult rv = clipboard->GetData(transferable, aWhichClipboard, nullptr);
  if (NS_FAILED(rv)) {
    return ClipboardResultPromise::CreateAndReject(rv, __func__);
  }

  auto resultPromise = MakeRefPtr<ClipboardResultPromise::Private>(__func__);

  auto contentAnalysisCallback =
      mozilla::MakeRefPtr<mozilla::contentanalysis::ContentAnalysis::
                              SafeContentAnalysisResultCallback>(
          [transferable, resultPromise,
           cpHandle = RefPtr{aRequestingContentParent}](
              RefPtr<nsIContentAnalysisResult>&& aResult) {
            // Needed to call cpHandle->GetContentParent()
            AssertIsOnMainThread();

            bool shouldAllow = aResult->GetShouldAllowContent();
            if (!shouldAllow) {
              resultPromise->Reject(NS_ERROR_CONTENT_BLOCKED, __func__);
              return;
            }
            dom::IPCTransferableData transferableData;
            RefPtr<dom::ContentParent> contentParent =
                cpHandle->GetContentParent();
            nsContentUtils::TransferableToIPCTransferableData(
                transferable, &transferableData, true /* aInSyncMessage */,
                contentParent);
            resultPromise->Resolve(std::move(transferableData), __func__);
          });

  contentanalysis::ContentAnalysis::CheckClipboardContentAnalysis(
      static_cast<nsBaseClipboard*>(clipboard.get()), window, transferable,
      aWhichClipboard, contentAnalysisCallback);
  return resultPromise;
}
}  // namespace

ipc::IPCResult ClipboardContentAnalysisParent::RecvGetClipboard(
    nsTArray<nsCString>&& aTypes, const int32_t& aWhichClipboard,
    const uint64_t& aRequestingWindowContextId,
    IPCTransferableDataOrError* aTransferableDataOrError) {
  // The whole point of having this actor is that it runs on a background thread
  // and so waiting for the content analysis result won't cause the main thread
  // to use SpinEventLoopUntil() which can cause a shutdownhang per bug 1901197.
  MOZ_ASSERT(!NS_IsMainThread());

  Monitor mon("ClipboardContentAnalysisParent::RecvGetClipboard");
  InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
              [&]() {
                return GetClipboardImpl(aTypes, aWhichClipboard,
                                        aRequestingWindowContextId,
                                        mThreadsafeContentParentHandle);
              })
      ->Then(GetMainThreadSerialEventTarget(), __func__,
             [&](ClipboardResultPromise::ResolveOrRejectValue&& aResult) {
               AssertIsOnMainThread();
               // Acquire the lock, pass the data back to the background
               // thread, and notify the background thread that work is
               // complete.
               MonitorAutoLock lock(mon);
               if (aResult.IsResolve()) {
                 *aTransferableDataOrError = std::move(aResult.ResolveValue());
               } else {
                 *aTransferableDataOrError = aResult.RejectValue();
               }
               mon.Notify();
             });

  {
    MonitorAutoLock lock(mon);
    while (aTransferableDataOrError->type() ==
           IPCTransferableDataOrError::T__None) {
      mon.Wait();
    }
  }

  if (aTransferableDataOrError->type() ==
      IPCTransferableDataOrError::Tnsresult) {
    NS_WARNING(nsPrintfCString(
                   "ClipboardContentAnalysisParent::"
                   "RecvGetClipboard got error %x",
                   static_cast<int>(aTransferableDataOrError->get_nsresult()))
                   .get());
  }

  return IPC_OK();
}
}  // namespace mozilla
