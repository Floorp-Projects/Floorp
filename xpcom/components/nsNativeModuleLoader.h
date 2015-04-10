/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeModuleLoader_h__
#define nsNativeModuleLoader_h__

#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "prlink.h"

namespace mozilla {
class FileLocation;
}

class nsNativeModuleLoader final
{
public:
  const mozilla::Module* LoadModule(mozilla::FileLocation& aFile);

  nsresult Init();

  void UnloadLibraries();

private:
  struct NativeLoadData
  {
    NativeLoadData() : mModule(nullptr), mLibrary(nullptr) {}

    const mozilla::Module* mModule;
    PRLibrary* mLibrary;
  };

  static PLDHashOperator
  ReleaserFunc(nsIHashable* aHashedFile, NativeLoadData& aLoadData, void*);

  static PLDHashOperator
  UnloaderFunc(nsIHashable* aHashedFile, NativeLoadData& aLoadData, void*);

  nsDataHashtable<nsHashableHashKey, NativeLoadData> mLibraries;
};

#endif /* nsNativeModuleLoader_h__ */
