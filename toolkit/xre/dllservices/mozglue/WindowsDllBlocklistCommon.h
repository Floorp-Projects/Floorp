/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsDllBlocklistCommon_h
#define mozilla_WindowsDllBlocklistCommon_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/WindowsDllBlocklistInfo.h"

#if !defined(DLL_BLOCKLIST_STRING_TYPE)
#  error "You must define DLL_BLOCKLIST_STRING_TYPE"
#endif  // !defined(DLL_BLOCKLIST_STRING_TYPE)

#define DLL_BLOCKLIST_DEFINITIONS_BEGIN_NAMED(name)                       \
  using DllBlockInfo = mozilla::DllBlockInfoT<DLL_BLOCKLIST_STRING_TYPE>; \
  static const DllBlockInfo name[] = {
#define DLL_BLOCKLIST_DEFINITIONS_BEGIN \
  DLL_BLOCKLIST_DEFINITIONS_BEGIN_NAMED(gWindowsDllBlocklist)

#define DLL_BLOCKLIST_DEFINITIONS_END \
  {}                                  \
  }                                   \
  ;

#define DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY_FOR(name, list) \
  const DllBlockInfo* name = &list[0]

#define DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(name) \
  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY_FOR(name, gWindowsDllBlocklist)

#define DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY_FOR(name, list) \
  const DllBlockInfo* name = &list[mozilla::ArrayLength(list) - 1]

#define DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(name) \
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY_FOR(name, gWindowsDllBlocklist)

#define DECLARE_DLL_BLOCKLIST_NUM_ENTRIES_FOR(name, list) \
  const size_t name = mozilla::ArrayLength(list) - 1

#define DECLARE_DLL_BLOCKLIST_NUM_ENTRIES(name) \
  DECLARE_DLL_BLOCKLIST_NUM_ENTRIES_FOR(name, gWindowsDllBlocklist)

#endif  // mozilla_WindowsDllBlocklistCommon_h
