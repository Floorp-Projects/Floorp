/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsEntropyCollector.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsSecurityWarningDialogs.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntropyCollector)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSecureBrowserUIImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSecurityWarningDialogs, Init)

static const nsModuleComponentInfo components[] =
{
  {
    "Entropy Collector",
    NS_ENTROPYCOLLECTOR_CID,
    NS_ENTROPYCOLLECTOR_CONTRACTID,
    nsEntropyCollectorConstructor
  },

  {
    "PSM Security Warnings",
    NS_SECURITYWARNINGDIALOGS_CID,
    NS_SECURITYWARNINGDIALOGS_CONTRACTID,
    nsSecurityWarningDialogsConstructor
  },

  {
    NS_SECURE_BROWSER_UI_CLASSNAME,
    NS_SECURE_BROWSER_UI_CID,
    NS_SECURE_BROWSER_UI_CONTRACTID,
    nsSecureBrowserUIImplConstructor
  }
};

NS_IMPL_NSGETMODULE(BOOT, components)
