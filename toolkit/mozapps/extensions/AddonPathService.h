//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AddonPathService_h
#define AddonPathService_h

#include "amIAddonPathService.h"
#include "nsString.h"
#include "nsTArray.h"

class nsIURI;
class JSAddonId;

namespace mozilla {

JSAddonId*
MapURIToAddonID(nsIURI* aURI);

class AddonPathService final : public amIAddonPathService
{
public:
  AddonPathService();

  static AddonPathService* GetInstance();

  JSAddonId* Find(const nsAString& path);
  static JSAddonId* FindAddonId(const nsAString& path);

  NS_DECL_ISUPPORTS
  NS_DECL_AMIADDONPATHSERVICE

  struct PathEntry
  {
    nsString mPath;
    JSAddonId* mAddonId;

    PathEntry(const nsAString& aPath, JSAddonId* aAddonId)
     : mPath(aPath), mAddonId(aAddonId)
    {}
  };

private:
  virtual ~AddonPathService();

  // Paths are stored sorted in order of their mPath.
  nsTArray<PathEntry> mPaths;

  static AddonPathService* sInstance;
};

} // namespace mozilla

#endif
