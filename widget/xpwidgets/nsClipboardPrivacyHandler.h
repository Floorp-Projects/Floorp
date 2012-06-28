/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipboardPrivacyHandler_h__
#define nsClipboardPrivacyHandler_h__

#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsITransferable;

// nsClipboardPrivacyHandler makes sure that clipboard data copied during
// the private browsing mode does not leak after exiting this mode.
// In order to ensure this, callers should store an object of this class
// for their lifetime, and call PrepareDataForClipboard in their
// nsIClipboard::SetData implementation before starting to use the
// nsITransferable object in any way.

class nsClipboardPrivacyHandler MOZ_FINAL : public nsIObserver,
                                            public nsSupportsWeakReference
{

public:

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver  
  NS_DECL_NSIOBSERVER

  nsresult Init();
  nsresult PrepareDataForClipboard(nsITransferable * aTransferable);
};

nsresult NS_NewClipboardPrivacyHandler(nsClipboardPrivacyHandler ** aHandler);

#endif // nsClipboardPrivacyHandler_h__
