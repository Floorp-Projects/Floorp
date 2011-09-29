/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#ifndef nsDeleteDir_h__
#define nsDeleteDir_h__

#include "nsCOMPtr.h"

class nsIFile;

/**
 * This routine attempts to delete a directory that may contain some files that
 * are still in use.  This later point is only an issue on Windows and a few
 * other systems.
 *
 * If the moveToTrash parameter is true, then the process for deleting the
 * directory creates a sibling directory of the same name with the ".Trash"
 * suffix.  It then attempts to move the given directory into the corresponding
 * trash folder (moving individual files if necessary).  Next, it proceeds to
 * delete each file in the trash folder on a low-priority background thread.
 *
 * If the moveToTrash parameter is false, then the given directory is deleted
 * directly.
 *
 * If the sync flag is true, then the delete operation runs to completion
 * before this function returns.  Otherwise, deletion occurs asynchronously.
 *
 * If 'delay' is non-zero, the directory will not be deleted until the
 * specified number of milliseconds have passed. (The directory is still
 * renamed immediately if 'moveToTrash' is passed, so upon return it is safe
 * to create a directory with the same name). This parameter is ignored if
 * 'sync' is true.
 */
NS_HIDDEN_(nsresult) DeleteDir(nsIFile *dir, bool moveToTrash, bool sync, 
                               PRUint32 delay = 0);

/**
 * This routine returns the trash directory corresponding to the given 
 * directory.
 */
NS_HIDDEN_(nsresult) GetTrashDir(nsIFile *dir, nsCOMPtr<nsIFile> *result);

#endif  // nsDeleteDir_h__
