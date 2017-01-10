/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOMGlue_h__
#define nsXPCOMGlue_h__

#include "nscore.h"

#ifdef XPCOM_GLUE

#include "mozilla/Bootstrap.h"

typedef void (*NSFuncPtr)();

namespace mozilla {

/**
 * Initialize the XPCOM glue by dynamically linking against the XPCOM
 * shared library indicated by xpcomFile and return a Bootstrap instance.
 */
NS_HIDDEN_(Bootstrap::UniquePtr) GetBootstrap(const char* aXPCOMFile);

} // namespace mozilla

#endif // XPCOM_GLUE
#endif // nsXPCOMGlue_h__
