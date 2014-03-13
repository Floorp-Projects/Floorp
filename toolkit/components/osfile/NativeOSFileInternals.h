/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nativeosfileinternalservice_h__
#define mozilla_nativeosfileinternalservice_h__

#include "nsINativeOSFileInternals.h"
#include "mozilla/Attributes.h"

namespace mozilla {

class NativeOSFileInternalsService MOZ_FINAL : public nsINativeOSFileInternalsService {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINATIVEOSFILEINTERNALSSERVICE
private:
  // Avoid accidental use of built-in operator=
  void operator=(const NativeOSFileInternalsService& other) MOZ_DELETE;
};

} // namespace mozilla

#endif // mozilla_finalizationwitnessservice_h__
