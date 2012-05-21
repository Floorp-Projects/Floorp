/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsDataHandler.h"

// The list of components we register
static const nsModuleComponentInfo components[] = {
    { "Data Protocol Handler", 
      NS_DATAHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "data", 
      nsDataHandler::Create},
};

NS_IMPL_NSGETMODULE(nsDataProtocolModule, components)



