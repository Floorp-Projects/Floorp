/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSBridge_h
#define NSSBridge_h

#include "nss.h"
#include "pk11func.h"
#include "pk11sdr.h"
#include "seccomon.h"
#include "secitem.h"
#include "secmodt.h"

#include "prerror.h"
#include "plstr.h"

#include <jni.h>

int setup_nss_functions(void *nss_handle, void *nssutil_handle, void *plc_handle);

#define NSS_WRAPPER(name, return_type, args...) \
typedef return_type (*name ## _t)(args);  \
extern name ## _t f_ ## name;

NSS_WRAPPER(NSS_Initialize, SECStatus, const char*, const char*, const char*, const char*, uint32_t)
NSS_WRAPPER(NSS_Shutdown, void, void)
NSS_WRAPPER(PK11SDR_Encrypt, SECStatus, SECItem *, SECItem *, SECItem *, void *)
NSS_WRAPPER(PK11SDR_Decrypt, SECStatus, SECItem *, SECItem *, void *)
NSS_WRAPPER(SECITEM_ZfreeItem, void, SECItem*, PRBool)
NSS_WRAPPER(PR_ErrorToString, char *, PRErrorCode, PRLanguageCode)
NSS_WRAPPER(PR_GetError, PRErrorCode, void)
NSS_WRAPPER(PR_Free, PRErrorCode, char *)
NSS_WRAPPER(PL_Base64Encode, char*, const char*, uint32_t, char*)
NSS_WRAPPER(PL_Base64Decode, char*, const char*, uint32_t, char*)
NSS_WRAPPER(PL_strfree, void, char*)
NSS_WRAPPER(PK11_GetInternalKeySlot, PK11SlotInfo *, void)
NSS_WRAPPER(PK11_NeedUserInit, PRBool, PK11SlotInfo *)
NSS_WRAPPER(PK11_InitPin, SECStatus, PK11SlotInfo*, const char*, const char*)

#endif /* NSS_h */
