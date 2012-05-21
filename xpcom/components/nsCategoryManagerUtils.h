/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCategoryManagerUtils_h__
#define nsCategoryManagerUtils_h__

#include "nsICategoryManager.h"

void
NS_CreateServicesFromCategory(const char *category,
                              nsISupports *origin,
                              const char *observerTopic);

#endif
