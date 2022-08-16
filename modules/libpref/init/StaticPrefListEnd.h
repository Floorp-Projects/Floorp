/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should be #included, along with StaticPrefListStart.h, in all
// headers that contribute prefs to the StaticPrefs namespace.

// This file does not make sense on its own. It must be #included along with
// StaticPrefsListBegin.h in all headers that contribute prefs to the
// StaticPrefs namespace.

#undef NEVER_PREF
#undef ALWAYS_PREF
#undef ALWAYS_DATAMUTEX_PREF
#undef ONCE_PREF

}  // namespace StaticPrefs
}  // namespace mozilla
