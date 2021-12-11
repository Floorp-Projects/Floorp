/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NtLoaderAPI_h
#define mozilla_NtLoaderAPI_h

#include "mozilla/LoaderAPIInterfaces.h"

#if !defined(IMPL_MFBT)
#  error "This should only be included from mozglue!"
#endif  // !defined(IMPL_MFBT)

namespace mozilla {

extern "C" MOZ_IMPORT_API nt::LoaderAPI* GetNtLoaderAPI(
    nt::LoaderObserver* aNewObserver);

}  // namespace mozilla

#endif  // mozilla_NtLoaderAPI_h
