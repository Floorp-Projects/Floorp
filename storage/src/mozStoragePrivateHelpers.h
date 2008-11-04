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

#ifndef _MOZSTORAGEPRIVATEHELPERS_H_
#define _MOZSTORAGEPRIVATEHELPERS_H_

#include "mozStorage.h"

/**
 * This file contains convenience methods for mozStorage.
 */

/**
 * Converts a SQLite return code to an nsresult return code.
 *
 * @param srv The SQLite return code.
 * @return The corresponding nsresult code.
 */
static nsresult
ConvertResultCode(int srv)
{
    switch (srv) {
        case SQLITE_OK:
        case SQLITE_ROW:
        case SQLITE_DONE:
            return NS_OK;
        case SQLITE_CORRUPT:
        case SQLITE_NOTADB:
            return NS_ERROR_FILE_CORRUPTED;
        case SQLITE_PERM:
        case SQLITE_CANTOPEN:
            return NS_ERROR_FILE_ACCESS_DENIED;
        case SQLITE_BUSY:
            return NS_ERROR_STORAGE_BUSY;
        case SQLITE_LOCKED:
            return NS_ERROR_FILE_IS_LOCKED;
        case SQLITE_READONLY:
            return NS_ERROR_FILE_READ_ONLY;
        case SQLITE_FULL:
        case SQLITE_TOOBIG:
            return NS_ERROR_FILE_NO_DEVICE_SPACE;
        case SQLITE_NOMEM:
            return NS_ERROR_OUT_OF_MEMORY;
        case SQLITE_MISUSE:
            return NS_ERROR_UNEXPECTED;
        case SQLITE_ABORT:
        case SQLITE_INTERRUPT:
            return NS_ERROR_ABORT;
    }

    // generic error
    return NS_ERROR_FAILURE;
}

#endif // _MOZSTORAGEPRIVATEHELPERS_H_

