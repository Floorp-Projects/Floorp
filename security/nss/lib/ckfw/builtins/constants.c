/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile$ $Revision$ $Date$";
#endif /* DEBUG */

/*
 * builtins/constants.c
 *
 * Identification and other constants, all collected here in one place.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef NSSCKBI_H
#include "nssckbi.h"
#endif /* NSSCKBI_H */

const CK_VERSION
nss_builtins_CryptokiVersion =  {
		NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR,
		NSS_BUILTINS_CRYPTOKI_VERSION_MINOR };

const CK_VERSION
nss_builtins_LibraryVersion = {
	NSS_BUILTINS_LIBRARY_VERSION_MAJOR,
	NSS_BUILTINS_LIBRARY_VERSION_MINOR};

const CK_VERSION
nss_builtins_HardwareVersion = { 
	NSS_BUILTINS_HARDWARE_VERSION_MAJOR,
	NSS_BUILTINS_HARDWARE_VERSION_MINOR };

const CK_VERSION
nss_builtins_FirmwareVersion = { 
	NSS_BUILTINS_FIRMWARE_VERSION_MAJOR,
	NSS_BUILTINS_FIRMWARE_VERSION_MINOR };

const NSSUTF8 
nss_builtins_ManufacturerID[] = { "Mozilla Foundation" };

const NSSUTF8 
nss_builtins_LibraryDescription[] = { "NSS Builtin Object Cryptoki Module" };

const NSSUTF8 
nss_builtins_SlotDescription[] = { "NSS Builtin Objects" };

const NSSUTF8 
nss_builtins_TokenLabel[] = { "Builtin Object Token" };

const NSSUTF8 
nss_builtins_TokenModel[] = { "1" };

/* should this be e.g. the certdata.txt RCS revision number? */
const NSSUTF8 
nss_builtins_TokenSerialNumber[] = { "1" };

