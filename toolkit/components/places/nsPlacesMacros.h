/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prtypes.h"

// Call a method on each observer in a category cache, then call the same
// method on the observer array.
#define NOTIFY_OBSERVERS(canFire, cache, array, type, method)                  \
  PR_BEGIN_MACRO                                                               \
  if (canFire) {                                                               \
    const nsCOMArray<type> &entries = cache.GetEntries();                      \
    for (PRInt32 idx = 0; idx < entries.Count(); ++idx)                        \
        entries[idx]->method;                                                  \
    ENUMERATE_WEAKARRAY(array, type, method)                                   \
  }                                                                            \
  PR_END_MACRO;

#define PLACES_FACTORY_SINGLETON_IMPLEMENTATION(_className, _sInstance)        \
  _className * _className::_sInstance = nsnull;                                \
                                                                               \
  _className *                                                                 \
  _className::GetSingleton()                                                   \
  {                                                                            \
    if (_sInstance) {                                                          \
      NS_ADDREF(_sInstance);                                                   \
      return _sInstance;                                                       \
    }                                                                          \
    _sInstance = new _className();                                             \
    if (_sInstance) {                                                          \
      NS_ADDREF(_sInstance);                                                   \
      if (NS_FAILED(_sInstance->Init())) {                                     \
        NS_RELEASE(_sInstance);                                                \
        _sInstance = nsnull;                                                   \
      }                                                                        \
    }                                                                          \
    return _sInstance;                                                         \
  }
