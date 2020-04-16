/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OSReauthenticator_h
#define OSReauthenticator_h

#include "nsIOSReauthenticator.h"

#define NS_OSREAUTHENTICATOR_CONTRACTID \
  "@mozilla.org/security/osreauthenticator;1"
#define NS_OSREAUTHENTICATOR_CID                     \
  {                                                  \
    0x4fe082ae, 0x6ff0, 0x4b41, {                    \
      0xb2, 0x4f, 0xea, 0xa6, 0x64, 0xf6, 0xe4, 0x6a \
    }                                                \
  }

class OSReauthenticator : public nsIOSReauthenticator {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOSREAUTHENTICATOR

 private:
  virtual ~OSReauthenticator() = default;
};

#ifdef XP_MACOSX
nsresult ReauthenticateUserMacOS(const nsAString& aPrompt,
                                 /* out */ bool& aReauthenticated,
                                 /* out */ bool& aIsBlankPassword);
#endif  // XP_MACOSX

#endif  // OSReauthenticator_h
