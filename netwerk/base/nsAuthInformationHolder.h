/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NSAUTHINFORMATIONHOLDER_H_
#define NSAUTHINFORMATIONHOLDER_H_

#include "nsIAuthInformation.h"
#include "nsString.h"

class nsAuthInformationHolder : public nsIAuthInformation {

protected:
    virtual ~nsAuthInformationHolder() {}

public:
    // aAuthType must be ASCII
    nsAuthInformationHolder(uint32_t aFlags, const nsString& aRealm,
                            const nsCString& aAuthType)
        : mFlags(aFlags), mRealm(aRealm), mAuthType(aAuthType) {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHINFORMATION

    const nsString& User() const { return mUser; }
    const nsString& Password() const { return mPassword; }
    const nsString& Domain() const { return mDomain; }

    /**
     * This method can be used to initialize the username when the
     * ONLY_PASSWORD flag is set.
     */
    void SetUserInternal(const nsString& aUsername) {
      mUser = aUsername;
    }
private:
    nsString mUser;
    nsString mPassword;
    nsString mDomain;

    uint32_t mFlags;
    nsString mRealm;
    nsCString mAuthType;
};


#endif
