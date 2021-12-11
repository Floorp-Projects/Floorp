/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the list of well-known PSM DataStorage classes that Gecko uses.
// These are key value data stores that are backed by a simple text-based
// storage in the profile directory.
//
// Please note that it is crucial for performance reasons for the number of
// these classes to remain low.  If you need to add to this list, you may
// need to update the algorithm in DataStorage::SetCachedStorageEntries()
// to something faster.

DATA_STORAGE(AlternateServices)
DATA_STORAGE(ClientAuthRememberList)
DATA_STORAGE(SiteSecurityServiceState)
