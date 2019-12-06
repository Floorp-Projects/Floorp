/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Call a method on each observer in a category cache, then call the same
// method on the observer array.
#define NOTIFY_OBSERVERS(canFire, array, type, method) \
  PR_BEGIN_MACRO                                       \
  if (canFire) {                                       \
    ENUMERATE_WEAKARRAY(array, type, method)           \
  }                                                    \
  PR_END_MACRO;

#define NOTIFY_BOOKMARKS_OBSERVERS(canFire, array, skipIf, method) \
  PR_BEGIN_MACRO                                                   \
  if (canFire) {                                                   \
    for (uint32_t idx = 0; idx < array.Length(); ++idx) {          \
      const nsCOMPtr<nsINavBookmarkObserver>& e =                  \
          array.ElementAt(idx).GetValue();                         \
      if (e) {                                                     \
        if (skipIf(e)) continue;                                   \
        e->method;                                                 \
      }                                                            \
    }                                                              \
  }                                                                \
  PR_END_MACRO;

#define PLACES_FACTORY_SINGLETON_IMPLEMENTATION(_className, _sInstance)     \
  _className* _className::_sInstance = nullptr;                             \
                                                                            \
  already_AddRefed<_className> _className::GetSingleton() {                 \
    if (_sInstance) {                                                       \
      RefPtr<_className> ret = _sInstance;                                  \
      return ret.forget();                                                  \
    }                                                                       \
    _sInstance = new _className();                                          \
    RefPtr<_className> ret = _sInstance;                                    \
    if (NS_FAILED(_sInstance->Init())) {                                    \
      /* Null out ret before _sInstance so the destructor doesn't assert */ \
      ret = nullptr;                                                        \
      _sInstance = nullptr;                                                 \
      return nullptr;                                                       \
    }                                                                       \
    return ret.forget();                                                    \
  }
