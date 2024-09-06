/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentAnalysis.h"
#include "mozilla/ClipboardContentAnalysisParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/Maybe.h"
#include "mozilla/Variant.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsBaseClipboard.h"
#include "nsIClipboard.h"
#include "nsID.h"
#include "nsITransferable.h"
#include "nsWidgetsCID.h"

namespace mozilla {
static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

ipc::IPCResult ClipboardContentAnalysisParent::RecvGetClipboard(
    nsTArray<nsCString>&& aTypes, const int32_t& aWhichClipboard,
    const uint64_t& aRequestingWindowContextId,
    IPCTransferableDataOrError* aTransferableDataOrError) {
  // The whole point of having this actor is that it runs on a background thread
  // and so waiting for the content analysis result won't cause the main thread
  // to use SpinEventLoopUntil() which can cause a shutdownhang per bug 1901197.
  MOZ_ASSERT(!NS_IsMainThread());
  RefPtr<nsIThread> actorThread = NS_GetCurrentThread();
  NS_ASSERTION(actorThread, "NS_GetCurrentThread() should not fail");
  // Ideally this would be a Maybe<Result>, but Result<> doesn't have a way to
  // get a reference to the IPCTransferableData inside it which makes it awkward
  // to use in this case.
  mozilla::Maybe<mozilla::Variant<IPCTransferableData, nsresult>>
      maybeTransferableResult;
  std::atomic<bool> transferableResultSet = false;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__, [actorThread, aTypes = std::move(aTypes), aWhichClipboard,
                 aRequestingWindowContextId, &maybeTransferableResult,
                 &transferableResultSet]() {
        nsresult rv = NS_OK;
        // Make sure we reply to the actor thread on error.
        auto sendRv = MakeScopeExit([&]() {
          maybeTransferableResult = Some(AsVariant(rv));
          transferableResultSet = true;
          // Wake up the actor thread so SpinEventLoopUntil() can check its
          // condition again.
          NS_DispatchToThreadQueue(NS_NewRunnableFunction(__func__, []() {}),
                                   actorThread, EventQueuePriority::Normal);
        });
        nsCOMPtr<nsIClipboard> clipboard;
        RefPtr<dom::WindowGlobalParent> window =
            dom::WindowGlobalParent::GetByInnerWindowId(
                aRequestingWindowContextId);
        // We expect content processes to always pass a non-null window so
        // Content Analysis can analyze it. (if Content Analysis is
        // active) There may be some cases when a window is closing, etc.,
        // in which case returning no clipboard content should not be a
        // problem.
        if (!window) {
          rv = NS_ERROR_FAILURE;
          return;
        }

        if (window->IsDiscarded()) {
          NS_WARNING(
              "discarded window passed to RecvGetClipboard(); returning "
              "no clipboard "
              "content");
          rv = NS_ERROR_FAILURE;
          return;
        }

        // Retrieve clipboard
        clipboard = do_GetService(kCClipboardCID, &rv);
        NS_ENSURE_SUCCESS_VOID(rv);

        auto transferableToCheck =
            dom::ContentParent::CreateClipboardTransferable(aTypes);
        if (transferableToCheck.isErr()) {
          rv = transferableToCheck.unwrapErr();
          return;
        }

        // Pass nullptr for the window here because we will be doing
        // content analysis ourselves asynchronously (so it doesn't block
        // main thread we're running on now)
        nsCOMPtr transferable = transferableToCheck.unwrap();
        rv = clipboard->GetData(transferable, aWhichClipboard, nullptr);
        NS_ENSURE_SUCCESS_VOID(rv);

        auto contentAnalysisCallback =
            mozilla::MakeRefPtr<mozilla::contentanalysis::ContentAnalysis::
                                    SafeContentAnalysisResultCallback>(
                [actorThread, transferable, aRequestingWindowContextId,
                 &maybeTransferableResult, &transferableResultSet](
                    RefPtr<nsIContentAnalysisResult>&& aResult) {
                  bool shouldAllow = aResult->GetShouldAllowContent();
                  if (!shouldAllow) {
                    maybeTransferableResult =
                        Some(AsVariant(NS_ERROR_CONTENT_BLOCKED));
                  } else {
                    IPCTransferableData transferableData;
                    RefPtr<dom::WindowGlobalParent> window =
                        dom::WindowGlobalParent::GetByInnerWindowId(
                            aRequestingWindowContextId);
                    if (!window && window->IsDiscarded()) {
                      maybeTransferableResult =
                          Some(AsVariant(NS_ERROR_UNEXPECTED));
                    } else {
                      maybeTransferableResult =
                          Some(AsVariant(IPCTransferableData()));
                      nsContentUtils::TransferableToIPCTransferableData(
                          transferable,
                          &(maybeTransferableResult.ref()
                                .as<IPCTransferableData>()),
                          true /* aInSyncMessage */,
                          window->BrowsingContext()->GetContentParent());
                    }
                  }
                  transferableResultSet = true;

                  // Setting maybeTransferableResult is done on the main thread
                  // instead of inside the runnable function here because some
                  // of the objects that can be inside a maybeTransferableResult
                  // are not thread-safe.
                  // Wake up the actor thread so SpinEventLoopUntil() can check
                  // its condition again.
                  NS_DispatchToThreadQueue(
                      NS_NewRunnableFunction(__func__, []() {}), actorThread,
                      EventQueuePriority::Normal);
                });

        contentanalysis::ContentAnalysis::CheckClipboardContentAnalysis(
            static_cast<nsBaseClipboard*>(clipboard.get()), window,
            transferable, aWhichClipboard, contentAnalysisCallback);

        sendRv.release();
      }));

  mozilla::SpinEventLoopUntil(
      "Waiting for clipboard and content analysis"_ns,
      [&transferableResultSet] { return transferableResultSet.load(); });

  NS_ASSERTION(maybeTransferableResult.isSome(),
               "maybeTransferableResult should be set when "
               "transferableResultSet is true!");
  auto& transferableResult = *maybeTransferableResult;
  if (transferableResult.is<nsresult>()) {
    *aTransferableDataOrError = transferableResult.as<nsresult>();
    NS_WARNING(
        nsPrintfCString("ClipboardContentAnalysisParent::"
                        "RecvGetClipboard got error %x",
                        static_cast<int>(transferableResult.as<nsresult>()))
            .get());
  } else {
    *aTransferableDataOrError =
        std::move(transferableResult.as<IPCTransferableData>());
  }

  return IPC_OK();
}
}  // namespace mozilla
