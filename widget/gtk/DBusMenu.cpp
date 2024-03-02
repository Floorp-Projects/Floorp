/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusMenu.h"
#include "prlink.h"
#include "nsThreadUtils.h"
#include "mozilla/ArrayUtils.h"

namespace mozilla::widget {

#define FUNC(name, type, params) \
  DBusMenuFunctions::_##name##_fn DBusMenuFunctions::s_##name;
DBUSMENU_GLIB_FUNCTIONS
DBUSMENU_GTK_FUNCTIONS
#undef FUNC

static PRLibrary* gDbusmenuGlib = nullptr;
static PRLibrary* gDbusmenuGtk = nullptr;

using DBusMenuFunc = void (*)();
struct DBusMenuDynamicFunction {
  const char* functionName;
  DBusMenuFunc* function;
};

static bool sInitialized;
static bool sLibPresent;

/* static */ bool DBusMenuFunctions::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitialized) {
    return sLibPresent;
  }
  sInitialized = true;
#define FUNC(name, type, params) \
  {#name, (DBusMenuFunc*)&DBusMenuFunctions::s_##name},
  static const DBusMenuDynamicFunction kDbusmenuGlibSymbols[] = {
      DBUSMENU_GLIB_FUNCTIONS};
  static const DBusMenuDynamicFunction kDbusmenuGtkSymbols[] = {
      DBUSMENU_GTK_FUNCTIONS};

#define LOAD_LIBRARY(symbol, name)                                            \
  if (!g##symbol) {                                                           \
    g##symbol = PR_LoadLibrary(name);                                         \
    if (!g##symbol) {                                                         \
      return false;                                                           \
    }                                                                         \
  }                                                                           \
  for (uint32_t i = 0; i < mozilla::ArrayLength(k##symbol##Symbols); ++i) {   \
    *k##symbol##Symbols[i].function =                                         \
        PR_FindFunctionSymbol(g##symbol, k##symbol##Symbols[i].functionName); \
    if (!*k##symbol##Symbols[i].function) {                                   \
      return false;                                                           \
    }                                                                         \
  }

  LOAD_LIBRARY(DbusmenuGlib, "libdbusmenu-glib.so.4")
  LOAD_LIBRARY(DbusmenuGtk, "libdbusmenu-gtk3.so.4")
#undef LOAD_LIBRARY

  sLibPresent = true;
  return true;
}

}  // namespace mozilla::widget
