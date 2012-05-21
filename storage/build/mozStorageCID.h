/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGECID_H
#define MOZSTORAGECID_H

#define MOZ_STORAGE_CONTRACTID_PREFIX "@mozilla.org/storage"


/* b71a1f84-3a70-4d37-a348-f1ba0e27eead */
#define MOZ_STORAGE_CONNECTION_CID \
{ 0xb71a1f84, 0x3a70, 0x4d37, {0xa3, 0x48, 0xf1, 0xba, 0x0e, 0x27, 0xee, 0xad} }

#define MOZ_STORAGE_CONNECTION_CONTRACTID MOZ_STORAGE_CONTRACTID_PREFIX "/connection;1"

/* bbbb1d61-438f-4436-92ed-8308e5830fb0 */
#define MOZ_STORAGE_SERVICE_CID \
{ 0xbbbb1d61, 0x438f, 0x4436, {0x92, 0xed, 0x83, 0x08, 0xe5, 0x83, 0x0f, 0xb0} }

#define MOZ_STORAGE_SERVICE_CONTRACTID MOZ_STORAGE_CONTRACTID_PREFIX "/service;1"

/* 3b667ee0-d2da-4ccc-9c3d-95f2ca6a8b4c */
#define VACUUMMANAGER_CID \
{ 0x3b667ee0, 0xd2da, 0x4ccc, { 0x9c, 0x3d, 0x95, 0xf2, 0xca, 0x6a, 0x8b, 0x4c } }

#define VACUUMMANAGER_CONTRACTID MOZ_STORAGE_CONTRACTID_PREFIX "/vacuum;1"

#endif /* MOZSTORAGECID_H */
