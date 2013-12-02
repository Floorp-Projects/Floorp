/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ExternalHelperAppChild_h
#define mozilla_dom_ExternalHelperAppChild_h

#include "mozilla/dom/PExternalHelperAppChild.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace dom {

class ExternalHelperAppChild : public PExternalHelperAppChild
                             , public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    ExternalHelperAppChild();
    virtual ~ExternalHelperAppChild();

    // Give the listener a real nsExternalAppHandler to complete processing on
    // the child.
    void SetHandler(nsIStreamListener *handler) { mHandler = handler; }

    virtual bool RecvCancel(const nsresult& aStatus);
private:
    nsCOMPtr<nsIStreamListener> mHandler;
    nsresult mStatus;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ExternalHelperAppChild_h
