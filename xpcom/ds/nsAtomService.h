/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAtomService_h
#define __nsAtomService_h

#include "nsIAtomService.h"
#include "mozilla/Attributes.h"

class nsAtomService MOZ_FINAL : public nsIAtomService
{
 public:
  nsAtomService();
  NS_DECL_THREADSAFE_ISUPPORTS
    
  NS_DECL_NSIATOMSERVICE

 private:
  ~nsAtomService() {}
};

#endif
