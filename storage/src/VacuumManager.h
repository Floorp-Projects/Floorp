/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_VacuumManager_h__
#define mozilla_storage_VacuumManager_h__

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStorageVacuumParticipant.h"
#include "nsCategoryCache.h"

namespace mozilla {
namespace storage {

class VacuumManager : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  VacuumManager();

  /**
   * Obtains the VacuumManager object.
   */
  static VacuumManager * getSingleton();

private:
  ~VacuumManager();

  static VacuumManager *gVacuumManager;

  // Cache of components registered in "vacuum-participant" category.
  nsCategoryCache<mozIStorageVacuumParticipant> mParticipants;
};

} // namespace storage
} // namespace mozilla

#endif
