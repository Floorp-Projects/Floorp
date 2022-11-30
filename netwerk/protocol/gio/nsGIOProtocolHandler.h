/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGIOProtocolHandler_h___
#define nsGIOProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "nsStringFwd.h"

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gGIOLog;

class nsGIOProtocolHandler final : public nsIProtocolHandler,
                                   public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsGIOProtocolHandler> GetSingleton();
  bool IsSupportedProtocol(const nsCString& aScheme);

 protected:
  ~nsGIOProtocolHandler() = default;

 private:
  nsresult Init();

  void InitSupportedProtocolsPref(nsIPrefBranch* prefs);

  static mozilla::StaticRefPtr<nsGIOProtocolHandler> sSingleton;
  nsTArray<nsCString> mSupportedProtocols;
};

#endif  // nsGIOProtocolHandler_h___
