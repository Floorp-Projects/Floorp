/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsString.h"

#include "nsEmbedCID.h"

#include "nsWebBrowser.h"
#include "nsWebBrowserContentPolicy.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserContentPolicy)

NS_DEFINE_NAMED_CID(NS_WEBBROWSERCONTENTPOLICY_CID);

static const mozilla::Module::CIDEntry kWebBrowserCIDs[] = {
  { &kNS_WEBBROWSERCONTENTPOLICY_CID, false, nullptr, nsWebBrowserContentPolicyConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWebBrowserContracts[] = {
  { NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, &kNS_WEBBROWSERCONTENTPOLICY_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kWebBrowserCategories[] = {
  { "content-policy", NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, NS_WEBBROWSERCONTENTPOLICY_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kWebBrowserModule = {
  mozilla::Module::kVersion,
  kWebBrowserCIDs,
  kWebBrowserContracts,
  kWebBrowserCategories
};

NSMODULE_DEFN(Browser_Embedding_Module) = &kWebBrowserModule;
