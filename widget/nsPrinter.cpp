/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinter.h"

nsPrinter::~nsPrinter() = default;

nsPrinter::nsPrinter(const nsAString& aName) : mName(aName) {}

// static
already_AddRefed<nsPrinter> nsPrinter::Create(const nsAString& aName) {
  return do_AddRef(new nsPrinter(aName));
}

NS_IMETHODIMP
nsPrinter::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}
