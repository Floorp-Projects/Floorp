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
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIFile;
class nsZipArchive;
class nsIURI;

namespace mozilla {

class Omnijar {
private:
/**
 * Store an nsIFile for an omni.jar. We can store two paths here, one
 * for GRE (corresponding to resource://gre/) and one for APP
 * (corresponding to resource:/// and resource://app/), but only
 * store one when both point to the same location (unified).
 */
static nsIFile *sPath[2];

/**
 * Cached nsZipArchives for the corresponding sPath
 */
static nsZipArchive *sReader[2];

/**
 * Has Omnijar::Init() been called?
 */
static bool sInitialized;

public:
enum Type {
    GRE = 0,
    APP = 1
};

/**
 * Returns whether SetBase has been called at least once with
 * a valid nsIFile
 */
static inline bool
IsInitialized()
{
    return sInitialized;
}

/**
 * Initializes the Omnijar API with the given directory or file for GRE and
 * APP. Each of the paths given can be:
 * - a file path, pointing to the omnijar file,
 * - a directory path, pointing to a directory containing an "omni.jar" file,
 * - nsnull for autodetection of an "omni.jar" file.
 */
static void Init(nsIFile *aGrePath = nsnull, nsIFile *aAppPath = nsnull);

/**
 * Cleans up the Omnijar API
 */
static void CleanUp();

/**
 * Returns an nsIFile pointing to the omni.jar file for GRE or APP.
 * Returns nsnull when there is no corresponding omni.jar.
 * Also returns nsnull for APP in the unified case.
 */
static inline already_AddRefed<nsIFile>
GetPath(Type aType)
{
    NS_ABORT_IF_FALSE(IsInitialized(), "Omnijar not initialized");
    NS_IF_ADDREF(sPath[aType]);
    return sPath[aType];
}

/**
 * Returns whether GRE or APP use an omni.jar. Returns PR_False for
 * APP when using an omni.jar in the unified case.
 */
static inline bool
HasOmnijar(Type aType)
{
    NS_ABORT_IF_FALSE(IsInitialized(), "Omnijar not initialized");
    return !!sPath[aType];
}

/**
 * Returns a nsZipArchive pointer for the omni.jar file for GRE or
 * APP. Returns nsnull in the same cases GetPath() would.
 */
static inline nsZipArchive *
GetReader(Type aType)
{
    NS_ABORT_IF_FALSE(IsInitialized(), "Omnijar not initialized");
    return sReader[aType];
}

/**
 * Returns a nsZipArchive pointer for the given path IAOI the given
 * path is the omni.jar for either GRE or APP.
 */
static nsZipArchive *GetReader(nsIFile *aPath);

/**
 * Returns the URI string corresponding to the omni.jar or directory
 * for GRE or APP. i.e. jar:/path/to/omni.jar!/ for omni.jar and
 * /path/to/base/dir/ otherwise. Returns an empty string for APP in
 * the unified case.
 * The returned URI is guaranteed to end with a slash.
 */
static nsresult GetURIString(Type aType, nsACString &result);

private:
/**
 * Used internally, respectively by Init() and CleanUp()
 */
static void InitOne(nsIFile *aPath, Type aType);
static void CleanUpOne(Type aType);

}; /* class Omnijar */

} /* namespace mozilla */

#endif /* mozilla_Omnijar_h */
