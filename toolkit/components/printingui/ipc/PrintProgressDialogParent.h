/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintProgressDialogParent_h
#define mozilla_embedding_PrintProgressDialogParent_h

#include "mozilla/embedding/PPrintProgressDialogParent.h"
#include "nsIObserver.h"

class nsIPrintProgressParams;
class nsIWebProgressListener;

namespace mozilla {
namespace embedding {
class PrintProgressDialogParent final : public PPrintProgressDialogParent,
                                        public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  MOZ_IMPLICIT PrintProgressDialogParent();

  void SetWebProgressListener(nsIWebProgressListener* aListener);

  void SetPrintProgressParams(nsIPrintProgressParams* aParams);

  virtual mozilla::ipc::IPCResult RecvStateChange(
      const long& stateFlags, const nsresult& status) override;

  virtual mozilla::ipc::IPCResult RecvProgressChange(
      const long& curSelfProgress, const long& maxSelfProgress,
      const long& curTotalProgress, const long& maxTotalProgress) override;

  virtual mozilla::ipc::IPCResult RecvDocTitleChange(
      const nsString& newTitle) override;

  virtual mozilla::ipc::IPCResult RecvDocURLChange(
      const nsString& newURL) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult Recv__delete__() override;

 private:
  virtual ~PrintProgressDialogParent();

  nsCOMPtr<nsIWebProgressListener> mWebProgressListener;
  nsCOMPtr<nsIPrintProgressParams> mPrintProgressParams;
  bool mActive;
  bool mDialogOpenedSent = false;
};

}  // namespace embedding
}  // namespace mozilla

#endif
