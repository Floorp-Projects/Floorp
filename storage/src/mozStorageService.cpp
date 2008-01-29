/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 sts=4
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Brett Wilson <brettw@gmail.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "mozStorageService.h"
#include "mozStorageConnection.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "plstr.h"

#include "sqlite3.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(mozStorageService, mozIStorageService)

mozStorageService *mozStorageService::gStorageService = nsnull;

mozStorageService *
mozStorageService::GetSingleton()
{
    if (gStorageService) {
        NS_ADDREF(gStorageService);
        return gStorageService;
    }
    
    gStorageService = new mozStorageService();
    if (gStorageService) {
        NS_ADDREF(gStorageService);
        if (NS_FAILED(gStorageService->Init()))
            NS_RELEASE(gStorageService);
    }
    
    return gStorageService;
}

mozStorageService::~mozStorageService()
{
    gStorageService = nsnull;
}

nsresult
mozStorageService::Init()
{
    // this makes multiple connections to the same database share the same pager
    // cache.
    sqlite3_enable_shared_cache(1);

    return NS_OK;
}

#ifndef NS_APP_STORAGE_50_FILE
#define NS_APP_STORAGE_50_FILE "UStor"
#endif

/* mozIStorageConnection openSpecialDatabase(in string aStorageKey); */
NS_IMETHODIMP
mozStorageService::OpenSpecialDatabase(const char *aStorageKey, mozIStorageConnection **_retval)
{
    nsresult rv;

    nsCOMPtr<nsIFile> storageFile;
    if (PL_strcmp(aStorageKey, "memory") == 0) {
        // just fall through with NULL storageFile, this will cause the storage
        // connection to use a memory DB.
    } else if (PL_strcmp(aStorageKey, "profile") == 0) {

        rv = NS_GetSpecialDirectory(NS_APP_STORAGE_50_FILE, getter_AddRefs(storageFile));
        if (NS_FAILED(rv)) {
            // teh wtf?
            return rv;
        }

        nsString filename;
        storageFile->GetPath(filename);
        nsCString filename8 = NS_ConvertUTF16toUTF8(filename.get());
        // fall through to DB initialization
    } else {
        return NS_ERROR_INVALID_ARG;
    }

    mozStorageConnection *msc = new mozStorageConnection(this);
    if (!msc)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = msc->Initialize (storageFile);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_retval = msc);
    return NS_OK;
}

/* mozIStorageConnection openDatabase(in nsIFile aDatabaseFile); */
NS_IMETHODIMP
mozStorageService::OpenDatabase(nsIFile *aDatabaseFile, mozIStorageConnection **_retval)
{
    nsresult rv;

    mozStorageConnection *msc = new mozStorageConnection(this);
    if (!msc)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = msc->Initialize (aDatabaseFile);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_retval = msc);
    return NS_OK;
}

/* mozIStorageConnection openUnsharedDatabase(in nsIFile aDatabaseFile); */
NS_IMETHODIMP
mozStorageService::OpenUnsharedDatabase(nsIFile *aDatabaseFile, mozIStorageConnection **_retval)
{
    nsresult rv;

    mozStorageConnection *msc = new mozStorageConnection(this);
    if (!msc)
        return NS_ERROR_OUT_OF_MEMORY;

    // Initialize the connection, temporarily turning off shared caches so the
    // new connection gets its own cache.  Database connections are assigned
    // caches when they are opened, and they retain those caches for their
    // lifetimes, unaffected by changes to the shared caches setting, so we can
    // disable shared caches temporarily while we initialize the new connection
    // without affecting the caches currently in use by other connections.
    sqlite3_enable_shared_cache(0);
    rv = msc->Initialize (aDatabaseFile);
    sqlite3_enable_shared_cache(1);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_retval = msc);
    return NS_OK;
}
