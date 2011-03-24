/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
 *   Mike Hommey <mh@glandium.org>
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

#ifndef mozilla_Omnijar_h
#define mozilla_Omnijar_h

#include "nscore.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIFile;
class nsZipArchive;
class nsIURI;

namespace mozilla {

#ifdef MOZ_ENABLE_LIBXUL
#define OMNIJAR_EXPORT
#else
#define OMNIJAR_EXPORT NS_EXPORT
#endif

class OMNIJAR_EXPORT Omnijar {
private:
/**
 * Store an nsIFile for either a base directory when there is no omni.jar,
 * or omni.jar itself. We can store two paths here, one for GRE
 * (corresponding to resource://gre/) and one for APP
 * (corresponding to resource:/// and resource://app/), but only
 * store one when both point to the same location (unified).
 */
static nsIFile *sPath[2];
/**
 * Store whether the corresponding sPath is an omni.jar or a directory
 */
static PRBool sIsOmnijar[2];

#ifdef MOZ_ENABLE_LIBXUL
/**
 * Cached nsZipArchives for the corresponding sPath
 */
static nsZipArchive *sReader[2];
#endif

public:
enum Type {
    GRE = 0,
    APP = 1
};

/**
 * Returns whether SetBase has been called at least once with
 * a valid nsIFile
 */
static PRBool
IsInitialized()
{
    // GRE path is always set after initialization.
    return sPath[0] != nsnull;
}

/**
 * Sets the base directories for GRE and APP. APP base directory
 * may be nsnull, in case the APP and GRE directories are the same.
 */
static nsresult SetBase(nsIFile *aGrePath, nsIFile *aAppPath);

/**
 * Returns an nsIFile pointing to the omni.jar file for GRE or APP.
 * Returns nsnull when there is no corresponding omni.jar.
 * Also returns nsnull for APP in the unified case.
 */
static already_AddRefed<nsIFile>
GetPath(Type aType)
{
    NS_ABORT_IF_FALSE(sPath[0], "Omnijar not initialized");

    if (sIsOmnijar[aType]) {
        NS_IF_ADDREF(sPath[aType]);
        return sPath[aType];
    }
    return nsnull;
}

/**
 * Returns whether GRE or APP use an omni.jar. Returns PR_False when
 * using an omni.jar in the unified case.
 */
static PRBool
HasOmnijar(Type aType)
{
    return sIsOmnijar[aType];
}

/**
 * Returns the base directory for GRE or APP. In the unified case,
 * returns nsnull for APP.
 */
static already_AddRefed<nsIFile> GetBase(Type aType);

/**
 * Returns a nsZipArchive pointer for the omni.jar file for GRE or
 * APP. Returns nsnull in the same cases GetPath() would.
 */
#ifdef MOZ_ENABLE_LIBXUL
static nsZipArchive *GetReader(Type aType);
#else
static nsZipArchive *GetReader(Type aType) { return nsnull; }
#endif

/**
 * Returns a nsZipArchive pointer for the given path IAOI the given
 * path is the omni.jar for either GRE or APP.
 */
#ifdef MOZ_ENABLE_LIBXUL
static nsZipArchive *GetReader(nsIFile *aPath);
#else
static nsZipArchive *GetReader(nsIFile *aPath) { return nsnull; }
#endif

/**
 * Returns the URI string corresponding to the omni.jar or directory
 * for GRE or APP. i.e. jar:/path/to/omni.jar!/ for omni.jar and
 * /path/to/base/dir/ otherwise. Returns an empty string for APP in
 * the unified case.
 * The returned URI is guaranteed to end with a slash.
 */
static nsresult GetURIString(Type aType, nsCString &result);

}; /* class Omnijar */

} /* namespace mozilla */

#endif /* mozilla_Omnijar_h */
