/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
