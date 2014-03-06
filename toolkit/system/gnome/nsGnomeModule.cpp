/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsToolkitCompsCID.h"
#include "mozilla/ModuleUtils.h"

#include <glib-object.h>

#ifdef MOZ_ENABLE_GCONF
#include "nsGConfService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGConfService, Init)
#endif
#ifdef MOZ_ENABLE_GNOMEVFS
#include "nsGnomeVFSService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGnomeVFSService, Init)
#endif
#ifdef MOZ_ENABLE_GIO
#include "nsGIOService.h"
#include "nsGSettingsService.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGIOService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGSettingsService, Init)
#endif

#ifdef MOZ_ENABLE_GCONF
NS_DEFINE_NAMED_CID(NS_GCONFSERVICE_CID);
#endif
#ifdef MOZ_ENABLE_GNOMEVFS
NS_DEFINE_NAMED_CID(NS_GNOMEVFSSERVICE_CID);
#endif
#ifdef MOZ_ENABLE_GIO
NS_DEFINE_NAMED_CID(NS_GIOSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_GSETTINGSSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kGnomeCIDs[] = {
#ifdef MOZ_ENABLE_GCONF
  { &kNS_GCONFSERVICE_CID, false, nullptr, nsGConfServiceConstructor },
#endif
#ifdef MOZ_ENABLE_GNOMEVFS
  { &kNS_GNOMEVFSSERVICE_CID, false, nullptr, nsGnomeVFSServiceConstructor },
#endif
#ifdef MOZ_ENABLE_GIO
  { &kNS_GIOSERVICE_CID, false, nullptr, nsGIOServiceConstructor },
  { &kNS_GSETTINGSSERVICE_CID, false, nullptr, nsGSettingsServiceConstructor },
#endif
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kGnomeContracts[] = {
#ifdef MOZ_ENABLE_GCONF
  { NS_GCONFSERVICE_CONTRACTID, &kNS_GCONFSERVICE_CID },
#endif
#ifdef MOZ_ENABLE_GNOMEVFS
  { NS_GNOMEVFSSERVICE_CONTRACTID, &kNS_GNOMEVFSSERVICE_CID },
#endif
#ifdef MOZ_ENABLE_GIO
  { NS_GIOSERVICE_CONTRACTID, &kNS_GIOSERVICE_CID },
  { NS_GSETTINGSSERVICE_CONTRACTID, &kNS_GSETTINGSSERVICE_CID },
#endif
  { nullptr }
};

static nsresult
InitGType ()
{
  g_type_init();
  return NS_OK;
}

static const mozilla::Module kGnomeModule = {
  mozilla::Module::kVersion,
  kGnomeCIDs,
  kGnomeContracts,
  nullptr,
  nullptr,
  InitGType
};

NSMODULE_DEFN(mozgnome) = &kGnomeModule;
