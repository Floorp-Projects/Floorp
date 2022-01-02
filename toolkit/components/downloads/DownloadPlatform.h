/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DownloadPlatform_h__
#define __DownloadPlatform_h__

#include "mozIDownloadPlatform.h"

#include "nsCOMPtr.h"

class nsIURI;

class DownloadPlatform : public mozIDownloadPlatform {
 protected:
  virtual ~DownloadPlatform() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIDOWNLOADPLATFORM

  static DownloadPlatform* gDownloadPlatformService;

  static DownloadPlatform* GetDownloadPlatform();

 private:
  static bool IsURLPossiblyFromWeb(nsIURI* aURI);
};

#endif
