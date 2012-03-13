#ifndef NSSBridge_h
#define NSSBridge_h

#include "nss.h"
#include "seccomon.h"
#include "secmodt.h"
#include "secutil.h"
#include "pk11func.h"
#include <jni.h>

int setup_nss_functions(void *nss_handle, void *nssutil_handle, void *plc_handle);

#define NSS_WRAPPER(name, return_type, args...) \
typedef return_type (*name ## _t)(args);  \
extern name ## _t f_ ## name;

NSS_WRAPPER(NSS_Initialize, SECStatus, const char*, const char*, const char*, const char*, PRUint32)
NSS_WRAPPER(NSS_Shutdown, void, void)
NSS_WRAPPER(PK11SDR_Encrypt, SECStatus, SECItem *, SECItem *, SECItem *, void *)
NSS_WRAPPER(PK11SDR_Decrypt, SECStatus, SECItem *, SECItem *, void *)
NSS_WRAPPER(SECITEM_ZfreeItem, void, SECItem*, PRBool)
NSS_WRAPPER(PR_ErrorToString, char *, PRErrorCode, PRLanguageCode)
NSS_WRAPPER(PR_GetError, PRErrorCode, void)
NSS_WRAPPER(PR_Free, PRErrorCode, char *)
NSS_WRAPPER(PL_Base64Encode, char*, const char*, PRUint32, char*)
NSS_WRAPPER(PL_Base64Decode, char*, const char*, PRUint32, char*)
NSS_WRAPPER(PL_strfree, void, char*)
NSS_WRAPPER(PK11_GetInternalKeySlot, PK11SlotInfo *, void)
NSS_WRAPPER(PK11_NeedUserInit, PRBool, PK11SlotInfo *)
NSS_WRAPPER(PK11_InitPin, SECStatus, PK11SlotInfo*, const char*, const char*)

bool setPassword(PK11SlotInfo *slot);
SECStatus doCrypto(JNIEnv* jenv, const char *path, const char *value, char** result, bool doEncrypt);
SECStatus encode(const unsigned char *data, PRInt32 dataLen, char **_retval);
SECStatus decode(const char *data, unsigned char **result, PRInt32 * _retval);
#endif /* NSS_h */
