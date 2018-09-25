/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Unused.h"
#include "nsIObserver.h"
#include "PrintProgressDialogChild.h"

class nsIWebProgress;
class nsIRequest;

using mozilla::Unused;

namespace mozilla {
namespace embedding {

NS_IMPL_ISUPPORTS(PrintProgressDialogChild,
                  nsIWebProgressListener,
                  nsIPrintProgressParams)

PrintProgressDialogChild::PrintProgressDialogChild(
  nsIObserver* aOpenObserver,
  nsIPrintSettings* aPrintSettings) :
  mOpenObserver(aOpenObserver),
  mPrintSettings(aPrintSettings)
{
}

PrintProgressDialogChild::~PrintProgressDialogChild()
{
  // When the printing engine stops supplying information about printing
  // progress, it'll drop references to us and destroy us. We need to signal
  // the parent to decrement its refcount, as well as prevent it from attempting
  // to contact us further.
  Unused << Send__delete__(this);
}

mozilla::ipc::IPCResult
PrintProgressDialogChild::RecvDialogOpened()
{
  // nsPrintJob's observer, which we're reporting to here, doesn't care
  // what gets passed as the subject, topic or data, so we'll just send
  // nullptrs.
  mOpenObserver->Observe(nullptr, nullptr, nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult
PrintProgressDialogChild::RecvCancelledCurrentJob()
{
  if (mPrintSettings) {
    mPrintSettings->SetIsCancelled(true);
  }
  return IPC_OK();
}

// nsIWebProgressListener

NS_IMETHODIMP
PrintProgressDialogChild::OnStateChange(nsIWebProgress* aProgress,
                                        nsIRequest* aRequest,
                                        uint32_t aStateFlags,
                                        nsresult aStatus)
{
  Unused << SendStateChange(aStateFlags, aStatus);
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnProgressChange(nsIWebProgress * aProgress,
                                           nsIRequest * aRequest,
                                           int32_t aCurSelfProgress,
                                           int32_t aMaxSelfProgress,
                                           int32_t aCurTotalProgress,
                                           int32_t aMaxTotalProgress)
{
  Unused << SendProgressChange(aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnLocationChange(nsIWebProgress* aProgress,
                                           nsIRequest* aRequest,
                                           nsIURI* aURI,
                                           uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnStatusChange(nsIWebProgress* aProgress,
                                         nsIRequest* aRequest,
                                         nsresult aStatus,
                                         const char16_t* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::OnSecurityChange(nsIWebProgress* aProgress,
                                           nsIRequest* aRequest,
                                           uint32_t aState)
{
  return NS_OK;
}

// nsIPrintProgressParams

NS_IMETHODIMP
PrintProgressDialogChild::GetDocTitle(nsAString& aDocTitle)
{
  aDocTitle = mDocTitle;
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::SetDocTitle(const nsAString& aDocTitle)
{
  mDocTitle = aDocTitle;
  Unused << SendDocTitleChange(PromiseFlatString(aDocTitle));
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::GetDocURL(nsAString& aDocURL)
{
  aDocURL = mDocURL;
  return NS_OK;
}

NS_IMETHODIMP
PrintProgressDialogChild::SetDocURL(const nsAString& aDocURL)
{
  mDocURL = aDocURL;
  Unused << SendDocURLChange(PromiseFlatString(aDocURL));
  return NS_OK;
}

} // namespace embedding
} // namespace mozilla
