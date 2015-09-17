/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPersistentProperties_h___
#define nsPersistentProperties_h___

#include "nsIPersistentProperties2.h"
#include "PLDHashTable.h"
#include "plarena.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsIUnicharInputStream;

class nsPersistentProperties final : public nsIPersistentProperties
{
public:
  nsPersistentProperties();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROPERTIES
  NS_DECL_NSIPERSISTENTPROPERTIES

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

private:
  ~nsPersistentProperties();

protected:
  nsCOMPtr<nsIUnicharInputStream> mIn;

  PLDHashTable mTable;
  PLArenaPool mArena;
};

class nsPropertyElement final : public nsIPropertyElement
{
public:
  nsPropertyElement()
  {
  }

  nsPropertyElement(const nsACString& aKey, const nsAString& aValue)
    : mKey(aKey)
    , mValue(aValue)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYELEMENT

  static NS_METHOD Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

private:
  ~nsPropertyElement() {}

protected:
  nsCString mKey;
  nsString mValue;
};

#endif /* nsPersistentProperties_h___ */
