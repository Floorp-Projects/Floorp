/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SHELLSERVICE_H__
#define __NS_SHELLSERVICE_H__

#include "nsIShellService.h"

class nsShellService MOZ_FINAL : public nsIShellService
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHELLSERVICE

  nsShellService() {}

private:
  ~nsShellService() {}
};

#define nsShellService_CID                          \
{0xae9ebe1c, 0x61e9, 0x45fa, {0x8f, 0x34, 0xc1, 0x07, 0x80, 0x3a, 0x5b, 0x44}}

#define nsShellService_ContractID "@mozilla.org/browser/shell-service;1"

#endif
