/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsString.h"

/**
 * @file nsStringGlue.h
 * This header exists solely to #include the proper internal/frozen string
 * headers, depending on whether MOZILLA_INTERNAL_API is defined.
 */

#ifndef nsStringGlue_h__
#define nsStringGlue_h__

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#include "nsReadableUtils.h"
#else
#include "nsStringAPI.h"
#endif

#endif // nsStringGlue_h__
