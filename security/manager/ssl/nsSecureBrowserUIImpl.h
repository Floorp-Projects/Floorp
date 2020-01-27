/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSecureBrowserUIImpl_h
#define nsSecureBrowserUIImpl_h

#include "nsCOMPtr.h"
#include "nsISecureBrowserUI.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsITransportSecurityInfo;
class nsIChannel;

namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

#define NS_SECURE_BROWSER_UI_CID                     \
  {                                                  \
    0xcc75499a, 0x1dd1, 0x11b2, {                    \
      0x8a, 0x82, 0xca, 0x41, 0x0a, 0xc9, 0x07, 0xb8 \
    }                                                \
  }

class nsSecureBrowserUIImpl : public nsISecureBrowserUI,
                              public nsIWebProgressListener,
                              public nsSupportsWeakReference {
 public:
  nsSecureBrowserUIImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSISECUREBROWSERUI

 protected:
  virtual ~nsSecureBrowserUIImpl(){};

  already_AddRefed<mozilla::dom::Document> PrepareForContentChecks();
  // Do mixed content checks. May update mState.
  void CheckForMixedContent();
  // Do Content Blocking checks. May update mEvent.
  void CheckForContentBlockingEvents();
  // Given some information about a request from an OnLocationChange event,
  // update mState and mTopLevelSecurityInfo.
  nsresult UpdateStateAndSecurityInfo(nsIChannel* channel, nsIURI* uri);

  uint32_t mState;
  uint32_t mEvent;
  bool mIsSecureContext;
  nsWeakPtr mDocShell;
  nsWeakPtr mWebProgress;
  nsCOMPtr<nsITransportSecurityInfo> mTopLevelSecurityInfo;
};

#endif  // nsSecureBrowserUIImpl_h
