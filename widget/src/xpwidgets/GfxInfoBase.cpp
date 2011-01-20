/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "GfxInfoBase.h"
#include "GfxInfoWebGL.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(GfxInfoBase, nsIGfxInfo)

#define BLACKLIST_PREF_BRANCH "gfx.blacklist."
#define SUGGESTED_VERSION_PREF BLACKLIST_PREF_BRANCH "suggested-driver-version"

static const char*
GetPrefNameForFeature(PRInt32 aFeature)
{
  const char* name = nsnull;
  switch(aFeature) {
    case nsIGfxInfo::FEATURE_DIRECT2D:
      name = BLACKLIST_PREF_BRANCH "direct2d";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d9";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d10";
      break;
    case nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.direct3d10-1";
      break;
    case nsIGfxInfo::FEATURE_OPENGL_LAYERS:
      name = BLACKLIST_PREF_BRANCH "layers.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_OPENGL:
      name = BLACKLIST_PREF_BRANCH "webgl.opengl";
      break;
    case nsIGfxInfo::FEATURE_WEBGL_ANGLE:
      name = BLACKLIST_PREF_BRANCH "webgl.angle";
      break;
    default:
      break;
  };

  return name;
}

// Returns the value of the pref for the relevant feature in aValue.
// If the pref doesn't exist, aValue is not touched, and returns false.
static bool
GetPrefValueForFeature(PRInt32 aFeature, PRInt32& aValue)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return false;

  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRInt32 val;
    if (NS_SUCCEEDED(prefs->GetIntPref(prefname, &val))) {
      aValue = val;
      return true;
    }
  }

  return false;
}

static void
SetPrefValueForFeature(PRInt32 aFeature, PRInt32 aValue)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return;

  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->SetIntPref(prefname, aValue);
  }
}

static void
RemovePrefForFeature(PRInt32 aFeature)
{
  const char *prefname = GetPrefNameForFeature(aFeature);
  if (!prefname)
    return;

  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->ClearUserPref(prefname);
  }
}

static bool
GetPrefValueForDriverVersion(nsACString& aVersion)
{
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsXPIDLCString version;
    if (NS_SUCCEEDED(prefs->GetCharPref(SUGGESTED_VERSION_PREF,
                                        getter_Copies(version)))) {
      aVersion = version;
      return true;
    }
  }

  return false;
}

static void
SetPrefValueForDriverVersion(const nsAString& aVersion)
{
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsCAutoString ver = NS_LossyConvertUTF16toASCII(aVersion);
    prefs->SetCharPref(SUGGESTED_VERSION_PREF,
                       PromiseFlatCString(ver).get());
  }
}

static void
RemovePrefForDriverVersion()
{
  nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->ClearUserPref(SUGGESTED_VERSION_PREF);
  }
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureStatus(PRInt32 aFeature, PRInt32* aStatus NS_OUTPARAM)
{
  if (GetPrefValueForFeature(aFeature, *aStatus))
    return NS_OK;

  nsString version;
  return GetFeatureStatusImpl(aFeature, aStatus, version);
}

NS_IMETHODIMP
GfxInfoBase::GetFeatureSuggestedDriverVersion(PRInt32 aFeature,
                                              nsAString& aVersion NS_OUTPARAM)
{
  nsCString version;
  if (GetPrefValueForDriverVersion(version)) {
    aVersion = NS_ConvertASCIItoUTF16(version);
    return NS_OK;
  }

  PRInt32 status;
  return GetFeatureStatusImpl(aFeature, &status, aVersion);
}


NS_IMETHODIMP
GfxInfoBase::GetWebGLParameter(const nsAString& aParam,
                               nsAString& aResult NS_OUTPARAM)
{
  return GfxInfoWebGL::GetWebGLParameter(aParam, aResult);
}
