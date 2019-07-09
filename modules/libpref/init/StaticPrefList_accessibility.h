/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file defines all the "accessibility.*" static prefs. It should only be
// included by StaticPrefs_accessibility.h.
//
// See StaticPrefList.h for details on how these entries are defined.

// clang-format off

VARCACHE_PREF(
  Live,
  "accessibility.monoaudio.enable",
   accessibility_monoaudio_enable,
  RelaxedAtomicBool, false
)

VARCACHE_PREF(
  Live,
  "accessibility.browsewithcaret",
   accessibility_browsewithcaret,
  RelaxedAtomicBool, false
)

// clang-format on
