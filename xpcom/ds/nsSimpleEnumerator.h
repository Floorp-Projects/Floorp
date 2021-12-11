/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSimpleEnumerator_h
#define nsSimpleEnumerator_h

#include "nsISimpleEnumerator.h"

class nsSimpleEnumerator : public nsISimpleEnumerator {
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATORBASE

  virtual const nsID& DefaultInterface() { return NS_GET_IID(nsISupports); }

 protected:
  virtual ~nsSimpleEnumerator() = default;
};

#endif
