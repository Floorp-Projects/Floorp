/* vim:t ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PendingTransactionQueue_h__
#define PendingTransactionQueue_h__

#include "nsClassHashtable.h"
#include "nsHttpTransaction.h"
#include "PendingTransactionInfo.h"

namespace mozilla {
namespace net {

class PendingTransactionQueue {
 public:
  PendingTransactionQueue() = default;

  void ReschedTransaction(nsHttpTransaction* aTrans);

  nsTArray<RefPtr<PendingTransactionInfo>>* GetTransactionPendingQHelper(
      nsAHttpTransaction* trans);

  void InsertTransactionSorted(
      nsTArray<RefPtr<PendingTransactionInfo>>& pendingQ,
      PendingTransactionInfo* pendingTransInfo,
      bool aInsertAsFirstForTheSamePriority = false);

  // Add a transaction information into the pending queue in
  // |mPendingTransactionTable| according to the transaction's
  // top level outer content window id.
  void InsertTransaction(PendingTransactionInfo* pendingTransInfo,
                         bool aInsertAsFirstForTheSamePriority = false);

  void AppendPendingUrgentStartQ(
      nsTArray<RefPtr<PendingTransactionInfo>>& result);

  // Append transactions to the |result| whose window id
  // is equal to |windowId|.
  // NOTE: maxCount == 0 will get all transactions in the queue.
  void AppendPendingQForFocusedWindow(
      uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
      uint32_t maxCount = 0);

  // Append transactions whose window id isn't equal to |windowId|.
  // NOTE: windowId == 0 will get all transactions for both
  // focused and non-focused windows.
  void AppendPendingQForNonFocusedWindows(
      uint64_t windowId, nsTArray<RefPtr<PendingTransactionInfo>>& result,
      uint32_t maxCount = 0);

  // Return the count of pending transactions for all window ids.
  size_t PendingQueueLength() const;
  size_t PendingQueueLengthForWindow(uint64_t windowId) const;

  // Remove the empty pendingQ in |mPendingTransactionTable|.
  void RemoveEmptyPendingQ();

  void PrintDiagnostics(nsCString& log);

  size_t UrgentStartQueueLength();

  void PrintPendingQ();

  void Compact();

  void CancelAllTransactions(nsresult reason);

  ~PendingTransactionQueue() = default;

 private:
  void InsertTransactionNormal(PendingTransactionInfo* info,
                               bool aInsertAsFirstForTheSamePriority = false);

  nsTArray<RefPtr<PendingTransactionInfo>>
      mUrgentStartQ;  // the urgent start transaction queue

  // This table provides a mapping from top level outer content window id
  // to a queue of pending transaction information.
  // The transaction's order in pending queue is decided by whether it's a
  // blocking transaction and its priority.
  // Note that the window id could be 0 if the http request
  // is initialized without a window.
  nsClassHashtable<nsUint64HashKey, nsTArray<RefPtr<PendingTransactionInfo>>>
      mPendingTransactionTable;
};

}  // namespace net
}  // namespace mozilla

#endif  // !PendingTransactionQueue_h__
