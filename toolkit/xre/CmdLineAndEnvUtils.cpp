/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"

namespace mozilla {

// Load the path of a file saved with SaveFileToEnv
already_AddRefed<nsIFile> GetFileFromEnv(const char* name) {
  nsresult rv;
  nsCOMPtr<nsIFile> file;

#ifdef XP_WIN
  WCHAR path[_MAX_PATH];
  if (!GetEnvironmentVariableW(NS_ConvertASCIItoUTF16(name).get(), path,
                               _MAX_PATH))
    return nullptr;

  rv = NS_NewLocalFile(nsDependentString(path), true, getter_AddRefs(file));
  if (NS_FAILED(rv)) return nullptr;

  return file.forget();
#else
  const char* arg = PR_GetEnv(name);
  if (!arg || !*arg) {
    return nullptr;
  }

  rv = NS_NewNativeLocalFile(nsDependentCString(arg), true,
                             getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return file.forget();
#endif
}

}  // namespace mozilla
