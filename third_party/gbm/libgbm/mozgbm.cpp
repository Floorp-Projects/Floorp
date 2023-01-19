/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"
#include "prlink.h"

#include <gbm/gbm.h>

#define GET_FUNC(func, lib)                                  \
    func##_fn =                                              \
      (decltype(func##_fn))PR_FindFunctionSymbol(lib, #func) \

#define IS_FUNC_LOADED(func)                                          \
    (func != nullptr)                                                 \

static struct gbm_device * (*gbm_create_device_fn)(int fd);
static void (*gbm_device_destroy_fn)(struct gbm_device* gbm);

bool IsGBMLibraryLoaded() {
  static bool isLoaded =
         (IS_FUNC_LOADED(gbm_create_device_fn) &&
          IS_FUNC_LOADED(gbm_device_destroy_fn));

  return isLoaded;
}

bool LoadGBMLibrary() {
  static PRLibrary* gbmLib = nullptr;
  static bool gbmInitialized = false;

  //TODO Thread safe
  if (!gbmInitialized) {
    gbmInitialized = true;
    gbmLib = PR_LoadLibrary("libgbm.so.1");
    if (!gbmLib) {
      return false;
    }

    GET_FUNC(gbm_create_device, gbmLib);
    GET_FUNC(gbm_device_destroy, gbmLib);
  }

  return IsGBMLibraryLoaded();
}

struct gbm_device *
gbm_create_device(int fd)
{
  if (!LoadGBMLibrary()) {
    return nullptr;
  }
  return gbm_create_device_fn(fd);
}

void
gbm_device_destroy(struct gbm_device* gbm)
{
  if (!LoadGBMLibrary()) {
    return;
  }
  return gbm_device_destroy_fn(gbm);
}
