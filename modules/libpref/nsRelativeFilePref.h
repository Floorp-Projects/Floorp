/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsRelativeFilePref_h
#define mozilla_nsRelativeFilePref_h

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"

// Note: This class is in its own file because it is needed by Mailnews.

namespace mozilla {

class nsRelativeFilePref final : public nsIRelativeFilePref {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRELATIVEFILEPREF

  nsRelativeFilePref();

 private:
  virtual ~nsRelativeFilePref();

  nsCOMPtr<nsIFile> mFile;
  nsCString mRelativeToKey;
};

}  // namespace mozilla

#endif  // mozilla_nsRelativeFilePref_h
