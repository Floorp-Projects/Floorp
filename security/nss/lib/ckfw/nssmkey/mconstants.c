/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nssmkey/constants.c
 *
 * Identification and other constants, all collected here in one place.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#include "nssmkey.h"

NSS_IMPLEMENT_DATA const CK_VERSION
    nss_ckmk_CryptokiVersion = {
        NSS_CKMK_CRYPTOKI_VERSION_MAJOR,
        NSS_CKMK_CRYPTOKI_VERSION_MINOR
    };

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_ManufacturerID = (NSSUTF8 *)"Mozilla Foundation";

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_LibraryDescription = (NSSUTF8 *)"NSS Access to Mac OS X Key Ring";

NSS_IMPLEMENT_DATA const CK_VERSION
    nss_ckmk_LibraryVersion = {
        NSS_CKMK_LIBRARY_VERSION_MAJOR,
        NSS_CKMK_LIBRARY_VERSION_MINOR
    };

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_SlotDescription = (NSSUTF8 *)"Mac OS X Key Ring";

NSS_IMPLEMENT_DATA const CK_VERSION
    nss_ckmk_HardwareVersion = {
        NSS_CKMK_HARDWARE_VERSION_MAJOR,
        NSS_CKMK_HARDWARE_VERSION_MINOR
    };

NSS_IMPLEMENT_DATA const CK_VERSION
    nss_ckmk_FirmwareVersion = {
        NSS_CKMK_FIRMWARE_VERSION_MAJOR,
        NSS_CKMK_FIRMWARE_VERSION_MINOR
    };

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_TokenLabel = (NSSUTF8 *)"Mac OS X Key Ring";

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_TokenModel = (NSSUTF8 *)"1";

NSS_IMPLEMENT_DATA const NSSUTF8 *
    nss_ckmk_TokenSerialNumber = (NSSUTF8 *)"1";
