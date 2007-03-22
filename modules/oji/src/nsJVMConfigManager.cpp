/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Sun Microsystems.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pete Zha <pete.zha@sun.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsJVMConfigManager.h"

NS_IMPL_ISUPPORTS1(nsJVMConfig, nsIJVMConfig)

nsJVMConfig::nsJVMConfig(const nsAString& aVersion, const nsAString& aType,
                         const nsAString& aOS, const nsAString& aArch,
                         nsIFile* aPath, nsIFile* aMozillaPluginPath,
                         const nsAString& aDescription) :
mVersion(aVersion),
mType(aType),
mOS(aOS),
mArch(aArch),
mPath(aPath),
mMozillaPluginPath(aMozillaPluginPath),
mDescription(aDescription)
{
}

nsJVMConfig::~nsJVMConfig()
{
}

/* readonly attribute AString version; */
NS_IMETHODIMP
nsJVMConfig::GetVersion(nsAString & aVersion)
{
    aVersion = mVersion;
    return NS_OK;
}

/* readonly attribute AString type; */
NS_IMETHODIMP
nsJVMConfig::GetType(nsAString & aType)
{
    aType = mType;
    return NS_OK;
}

/* readonly attribute AString os; */
NS_IMETHODIMP
nsJVMConfig::GetOS(nsAString & aOS)
{
    aOS = mOS;
    return NS_OK;
}

/* readonly attribute AString arch; */
NS_IMETHODIMP
nsJVMConfig::GetArch(nsAString & aArch)
{
    aArch = mArch;
    return NS_OK;
}

NS_IMETHODIMP
nsJVMConfig::GetPath(nsIFile** aPath)
{
    NS_ENSURE_ARG_POINTER(aPath);

    *aPath = mPath;
    NS_IF_ADDREF(*aPath);
    return NS_OK;
}

NS_IMETHODIMP
nsJVMConfig::GetMozillaPluginPath(nsIFile** aMozillaPluginPath)
{
    NS_ENSURE_ARG_POINTER(aMozillaPluginPath);

    *aMozillaPluginPath = mMozillaPluginPath;
    NS_IF_ADDREF(*aMozillaPluginPath);
    return NS_OK;
}

/* readonly attribute AString description; */
NS_IMETHODIMP
nsJVMConfig::GetDescription(nsAString & aDescription)
{
    aDescription = mDescription;
    return NS_OK;
}

