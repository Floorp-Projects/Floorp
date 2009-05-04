/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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

#ifndef _MOZSTORAGESERVICE_H_
#define _MOZSTORAGESERVICE_H_

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "prlock.h"

#include "mozIStorageService.h"

class nsIXPConnect;

namespace mozilla {
namespace storage {

class Service : public mozIStorageService
              , public nsIObserver
{
public:
  /**
   * Initializes the service.  This must be called before any other function!
   */
  nsresult initialize();

  static Service *getSingleton();

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESERVICE
  NS_DECL_NSIOBSERVER

  /**
   * Obtains an already AddRefed pointer to XPConnect.  This is used by
   * language helpers.
   */
  static already_AddRefed<nsIXPConnect> getXPConnect();

private:
  virtual ~Service();

  /**
   * Used for locking around calls when initializing connections so that we
   * can ensure that the state of sqlite3_enable_shared_cache is sane.
   */
  PRLock *mLock;

  /**
   * Shuts down the storage service, freeing all of the acquired resources.
   */
  void shutdown();

  nsCOMPtr<nsIFile> mProfileStorageFile;

  static Service *gService;

  static nsIXPConnect *sXPConnect;
};

} // namespace storage
} // namespace mozilla

#endif /* _MOZSTORAGESERVICE_H_ */
