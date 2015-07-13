/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ErrorNames_h
#define mozilla_ErrorNames_h

#include "nsError.h"

class nsACString;

namespace mozilla {

// Maps the given nsresult to its symbolic name. For example,
// GetErrorName(NS_OK, name) will result in name == "NS_OK".
// When the symbolic name is unknown, name will be of the form
// "NS_ERROR_GENERATE_SUCCESS(<module>, <code>)" or
// "NS_ERROR_GENERATE_FAILURE(<module>, <code>)".
void GetErrorName(nsresult rv, nsACString& name);

} // namespace mozilla

#endif // mozilla_ErrorNames_h
