/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTerminator_h__
#define nsTerminator_h__

#include "nsISupports.h"
#include "nsIObserver.h"

namespace mozilla {

class nsTerminator MOZ_FINAL: public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsTerminator();

private:
  nsresult SelfInit();
  void Start();

  ~nsTerminator() {}

  bool mInitialized;
};

}

#define NS_TOOLKIT_TERMINATOR_CID { 0x2e59cc70, 0xf83a, 0x412f, \
  { 0x89, 0xd4, 0x45, 0x38, 0x85, 0x83, 0x72, 0x17 } }
#define NS_TOOLKIT_TERMINATOR_CONTRACTID "@mozilla.org/toolkit/shutdown-terminator;1"

#endif // nsTerminator_h__
