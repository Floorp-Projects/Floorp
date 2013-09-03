/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DownloadPlatform_h__
#define __DownloadPlatform_h__

#include "mozIDownloadPlatform.h"

#include "nsCOMPtr.h"

class nsIURI;
class nsIFile;

class DownloadPlatform : public mozIDownloadPlatform
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_MOZIDOWNLOADPLATFORM

  DownloadPlatform() { }
  virtual ~DownloadPlatform() { }

  static DownloadPlatform *gDownloadPlatformService;

  static DownloadPlatform* GetDownloadPlatform();
};

#endif
