/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * This file implements PKCS 11 on top of our existing security modules
 *
 * For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *   This implementation has two slots:
 *  slot 1 is our generic crypto support. It does not require login
 *   (unless you've enabled FIPS). It supports Public Key ops, and all they
 *   bulk ciphers and hashes. It can also support Private Key ops for imported
 *   Private keys. It does not have any token storage.
 *  slot 2 is our private key support. It requires a login before use. It
 *   can store Private Keys and Certs as token objects. Currently only private
 *   keys and their associated Certificates are saved on the token.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */
#include "seccomon.h"
#include "softoken.h"
#include "lowkeyi.h"
#include "pkcs11.h"
#include "pkcs11i.h"
#include "prenv.h"
#include "prprf.h"

#include <ctype.h>

#ifdef XP_UNIX
#define NSS_AUDIT_WITH_SYSLOG 1
#include <syslog.h>
#include <unistd.h>
#endif

#ifdef LINUX
#include <pthread.h>
#include <dlfcn.h>
#define LIBAUDIT_NAME "libaudit.so.1"
#ifndef AUDIT_CRYPTO_TEST_USER
#define AUDIT_CRYPTO_TEST_USER 2400         /* Crypto test results */
#define AUDIT_CRYPTO_PARAM_CHANGE_USER 2401 /* Crypto attribute change */
#define AUDIT_CRYPTO_LOGIN 2402             /* Logged in as crypto officer */
#define AUDIT_CRYPTO_LOGOUT 2403            /* Logged out from crypto */
#define AUDIT_CRYPTO_KEY_USER 2404          /* Create,delete,negotiate */
#define AUDIT_CRYPTO_FAILURE_USER 2405      /* Fail decrypt,encrypt,randomize */
#endif
static void *libaudit_handle;
static int (*audit_open_func)(void);
static void (*audit_close_func)(int fd);
static int (*audit_log_user_message_func)(int audit_fd, int type,
                                          const char *message, const char *hostname, const char *addr,
                                          const char *tty, int result);
static int (*audit_send_user_message_func)(int fd, int type,
                                           const char *message);

static pthread_once_t libaudit_once_control = PTHREAD_ONCE_INIT;

static void
libaudit_init(void)
{
    libaudit_handle = dlopen(LIBAUDIT_NAME, RTLD_LAZY);
    if (!libaudit_handle) {
        return;
    }
    audit_open_func = dlsym(libaudit_handle, "audit_open");
    audit_close_func = dlsym(libaudit_handle, "audit_close");
    /*
     * audit_send_user_message is the older function.
     * audit_log_user_message, if available, is preferred.
     */
    audit_log_user_message_func = dlsym(libaudit_handle,
                                        "audit_log_user_message");
    if (!audit_log_user_message_func) {
        audit_send_user_message_func = dlsym(libaudit_handle,
                                             "audit_send_user_message");
    }
    if (!audit_open_func || !audit_close_func ||
        (!audit_log_user_message_func && !audit_send_user_message_func)) {
        dlclose(libaudit_handle);
        libaudit_handle = NULL;
        audit_open_func = NULL;
        audit_close_func = NULL;
        audit_log_user_message_func = NULL;
        audit_send_user_message_func = NULL;
    }
}
#endif /* LINUX */

/*
 * ******************** Password Utilities *******************************
 */
static PRBool isLoggedIn = PR_FALSE;
static PRBool isLevel2 = PR_TRUE;
PRBool sftk_fatalError = PR_FALSE;

/*
 * This function returns
 *   - CKR_PIN_INVALID if the password/PIN is not a legal UTF8 string
 *   - CKR_PIN_LEN_RANGE if the password/PIN is too short or does not
 *     consist of characters from three or more character classes.
 *   - CKR_OK otherwise
 *
 * The minimum password/PIN length is FIPS_MIN_PIN Unicode characters.
 * We define five character classes: digits (0-9), ASCII lowercase letters,
 * ASCII uppercase letters, ASCII non-alphanumeric characters (such as
 * space and punctuation marks), and non-ASCII characters.  If an ASCII
 * uppercase letter is the first character of the password/PIN, the
 * uppercase letter is not counted toward its character class.  Similarly,
 * if a digit is the last character of the password/PIN, the digit is not
 * counted toward its character class.
 *
 * Although NSC_SetPIN and NSC_InitPIN already do the maximum and minimum
 * password/PIN length checks, they check the length in bytes as opposed
 * to characters.  To meet the minimum password/PIN guessing probability
 * requirements in FIPS 140-2, we need to check the length in characters.
 */
static CK_RV
sftk_newPinCheck(CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    unsigned int i;
    int nchar = 0;     /* number of characters */
    int ntrail = 0;    /* number of trailing bytes to follow */
    int ndigit = 0;    /* number of decimal digits */
    int nlower = 0;    /* number of ASCII lowercase letters */
    int nupper = 0;    /* number of ASCII uppercase letters */
    int nnonalnum = 0; /* number of ASCII non-alphanumeric characters */
    int nnonascii = 0; /* number of non-ASCII characters */
    int nclass;        /* number of character classes */

    for (i = 0; i < ulPinLen; i++) {
        unsigned int byte = pPin[i];

        if (ntrail) {
            if ((byte & 0xc0) != 0x80) {
                /* illegal */
                nchar = -1;
                break;
            }
            if (--ntrail == 0) {
                nchar++;
                nnonascii++;
            }
            continue;
        }
        if ((byte & 0x80) == 0x00) {
            /* single-byte (ASCII) character */
            nchar++;
            if (isdigit(byte)) {
                if (i < ulPinLen - 1) {
                    ndigit++;
                }
            } else if (islower(byte)) {
                nlower++;
            } else if (isupper(byte)) {
                if (i > 0) {
                    nupper++;
                }
            } else {
                nnonalnum++;
            }
        } else if ((byte & 0xe0) == 0xc0) {
            /* leading byte of two-byte character */
            ntrail = 1;
        } else if ((byte & 0xf0) == 0xe0) {
            /* leading byte of three-byte character */
            ntrail = 2;
        } else if ((byte & 0xf8) == 0xf0) {
            /* leading byte of four-byte character */
            ntrail = 3;
        } else {
            /* illegal */
            nchar = -1;
            break;
        }
    }
    if (nchar == -1) {
        /* illegal UTF8 string */
        return CKR_PIN_INVALID;
    }
    if (nchar < FIPS_MIN_PIN) {
        return CKR_PIN_LEN_RANGE;
    }
    nclass = (ndigit != 0) + (nlower != 0) + (nupper != 0) +
             (nnonalnum != 0) + (nnonascii != 0);
    if (nclass < 3) {
        return CKR_PIN_LEN_RANGE;
    }
    return CKR_OK;
}

/* FIPS required checks before any useful cryptographic services */
static CK_RV
sftk_fipsCheck(void)
{
    if (sftk_fatalError)
        return CKR_DEVICE_ERROR;
    if (isLevel2 && !isLoggedIn)
        return CKR_USER_NOT_LOGGED_IN;
    return CKR_OK;
}

#define SFTK_FIPSCHECK()                   \
    CK_RV rv;                              \
    if ((rv = sftk_fipsCheck()) != CKR_OK) \
        return rv;

#define SFTK_FIPSFATALCHECK() \
    if (sftk_fatalError)      \
        return CKR_DEVICE_ERROR;

/* grab an attribute out of a raw template */
void *
fc_getAttribute(CK_ATTRIBUTE_PTR pTemplate,
                CK_ULONG ulCount, CK_ATTRIBUTE_TYPE type)
{
    int i;

    for (i = 0; i < (int)ulCount; i++) {
        if (pTemplate[i].type == type) {
            return pTemplate[i].pValue;
        }
    }
    return NULL;
}

#define __PASTE(x, y) x##y

/* ------------- forward declare all the NSC_ functions ------------- */
#undef CK_NEED_ARG_LIST
#undef CK_PKCS11_FUNCTION_INFO

#define CK_PKCS11_FUNCTION_INFO(name) CK_RV __PASTE(NS, name)
#define CK_NEED_ARG_LIST 1

#include "pkcs11f.h"

/* ------------- forward declare all the FIPS functions ------------- */
#undef CK_NEED_ARG_LIST
#undef CK_PKCS11_FUNCTION_INFO

#define CK_PKCS11_FUNCTION_INFO(name) CK_RV __PASTE(F, name)
#define CK_NEED_ARG_LIST 1

#include "pkcs11f.h"

/* ------------- build the CK_CRYPTO_TABLE ------------------------- */
static CK_FUNCTION_LIST sftk_fipsTable = {
    { 1, 10 },

#undef CK_NEED_ARG_LIST
#undef CK_PKCS11_FUNCTION_INFO

#define CK_PKCS11_FUNCTION_INFO(name) \
    __PASTE(F, name)                  \
    ,

#include "pkcs11f.h"

};

#undef CK_NEED_ARG_LIST
#undef CK_PKCS11_FUNCTION_INFO

#undef __PASTE

/* CKO_NOT_A_KEY can be any object class that's not a key object. */
#define CKO_NOT_A_KEY CKO_DATA

#define SFTK_IS_KEY_OBJECT(objClass)    \
    (((objClass) == CKO_PUBLIC_KEY) ||  \
     ((objClass) == CKO_PRIVATE_KEY) || \
     ((objClass) == CKO_SECRET_KEY))

#define SFTK_IS_NONPUBLIC_KEY_OBJECT(objClass) \
    (((objClass) == CKO_PRIVATE_KEY) || ((objClass) == CKO_SECRET_KEY))

static CK_RV
sftk_get_object_class_and_fipsCheck(CK_SESSION_HANDLE hSession,
                                    CK_OBJECT_HANDLE hObject, CK_OBJECT_CLASS *pObjClass)
{
    CK_RV rv;
    CK_ATTRIBUTE class;
    class.type = CKA_CLASS;
    class.pValue = pObjClass;
    class.ulValueLen = sizeof(*pObjClass);
    rv = NSC_GetAttributeValue(hSession, hObject, &class, 1);
    if ((rv == CKR_OK) && SFTK_IS_NONPUBLIC_KEY_OBJECT(*pObjClass)) {
        rv = sftk_fipsCheck();
    }
    return rv;
}

#ifdef LINUX

int
sftk_mapLinuxAuditType(NSSAuditSeverity severity, NSSAuditType auditType)
{
    switch (auditType) {
        case NSS_AUDIT_ACCESS_KEY:
        case NSS_AUDIT_CHANGE_KEY:
        case NSS_AUDIT_COPY_KEY:
        case NSS_AUDIT_DERIVE_KEY:
        case NSS_AUDIT_DESTROY_KEY:
        case NSS_AUDIT_DIGEST_KEY:
        case NSS_AUDIT_GENERATE_KEY:
        case NSS_AUDIT_LOAD_KEY:
        case NSS_AUDIT_UNWRAP_KEY:
        case NSS_AUDIT_WRAP_KEY:
            return AUDIT_CRYPTO_KEY_USER;
        case NSS_AUDIT_CRYPT:
            return (severity == NSS_AUDIT_ERROR) ? AUDIT_CRYPTO_FAILURE_USER : AUDIT_CRYPTO_KEY_USER;
        case NSS_AUDIT_FIPS_STATE:
        case NSS_AUDIT_INIT_PIN:
        case NSS_AUDIT_INIT_TOKEN:
        case NSS_AUDIT_SET_PIN:
            return AUDIT_CRYPTO_PARAM_CHANGE_USER;
        case NSS_AUDIT_SELF_TEST:
            return AUDIT_CRYPTO_TEST_USER;
        case NSS_AUDIT_LOGIN:
            return AUDIT_CRYPTO_LOGIN;
        case NSS_AUDIT_LOGOUT:
            return AUDIT_CRYPTO_LOGOUT;
            /* we skip the fault case here so we can get compiler
             * warnings if new 'NSSAuditType's are added without
             * added them to this list, defaults fall through */
    }
    /* default */
    return AUDIT_CRYPTO_PARAM_CHANGE_USER;
}
#endif

/**********************************************************************
 *
 *     FIPS 140 auditable event logging
 *
 **********************************************************************/

PRBool sftk_audit_enabled = PR_FALSE;

/*
 * Each audit record must have the following information:
 * - Date and time of the event
 * - Type of event
 * - user (subject) identity
 * - outcome (success or failure) of the event
 * - process ID
 * - name (ID) of the object
 * - for changes to data (except for authentication data and CSPs), the new
 *   and old values of the data
 * - for authentication attempts, the origin of the attempt (e.g., terminal
 *   identifier)
 * - for assuming a role, the type of role, and the location of the request
 */
void
sftk_LogAuditMessage(NSSAuditSeverity severity, NSSAuditType auditType,
                     const char *msg)
{
#ifdef NSS_AUDIT_WITH_SYSLOG
    int level;

    switch (severity) {
        case NSS_AUDIT_ERROR:
            level = LOG_ERR;
            break;
        case NSS_AUDIT_WARNING:
            level = LOG_WARNING;
            break;
        default:
            level = LOG_INFO;
            break;
    }
    /* timestamp is provided by syslog in the message header */
    syslog(level | LOG_USER /* facility */,
           "NSS " SOFTOKEN_LIB_NAME "[pid=%d uid=%d]: %s",
           (int)getpid(), (int)getuid(), msg);
#ifdef LINUX
    if (pthread_once(&libaudit_once_control, libaudit_init) != 0) {
        return;
    }
    if (libaudit_handle) {
        int audit_fd;
        int linuxAuditType;
        int result = (severity != NSS_AUDIT_ERROR); /* 1=success; 0=failed */
        char *message = PR_smprintf("NSS " SOFTOKEN_LIB_NAME ": %s", msg);
        if (!message) {
            return;
        }
        audit_fd = audit_open_func();
        if (audit_fd < 0) {
            PR_smprintf_free(message);
            return;
        }
        linuxAuditType = sftk_mapLinuxAuditType(severity, auditType);
        if (audit_log_user_message_func) {
            audit_log_user_message_func(audit_fd, linuxAuditType, message,
                                        NULL, NULL, NULL, result);
        } else {
            audit_send_user_message_func(audit_fd, linuxAuditType, message);
        }
        audit_close_func(audit_fd);
        PR_smprintf_free(message);
    }
#endif /* LINUX */
#else
/* do nothing */
#endif
}

/**********************************************************************
 *
 *     Start of PKCS 11 functions
 *
 **********************************************************************/
/* return the function list */
CK_RV
FC_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{

    CHECK_FORK();

    *pFunctionList = &sftk_fipsTable;
    return CKR_OK;
}

/* sigh global so pkcs11 can read it */
PRBool nsf_init = PR_FALSE;

void
fc_log_init_error(CK_RV crv)
{
    if (sftk_audit_enabled) {
        char msg[128];
        PR_snprintf(msg, sizeof msg,
                    "C_Initialize()=0x%08lX "
                    "power-up self-tests failed",
                    (PRUint32)crv);
        sftk_LogAuditMessage(NSS_AUDIT_ERROR, NSS_AUDIT_SELF_TEST, msg);
    }
}

/* FC_Initialize initializes the PKCS #11 library. */
CK_RV
FC_Initialize(CK_VOID_PTR pReserved)
{
    const char *envp;
    CK_RV crv;

    if ((envp = PR_GetEnv("NSS_ENABLE_AUDIT")) != NULL) {
        sftk_audit_enabled = (atoi(envp) == 1);
    }

    /* At this point we should have already done post and integrity checks.
     * if we haven't, it probably means the FIPS product has not been installed
     * or the tests failed. Don't let an application try to enter FIPS mode */
    crv = sftk_FIPSEntryOK();
    if (crv != CKR_OK) {
        sftk_fatalError = PR_TRUE;
        fc_log_init_error(crv);
        return crv;
    }

    sftk_ForkReset(pReserved, &crv);

    if (nsf_init) {
        return CKR_CRYPTOKI_ALREADY_INITIALIZED;
    }

    crv = nsc_CommonInitialize(pReserved, PR_TRUE);

    /* not an 'else' rv can be set by either SFTK_LowInit or SFTK_SlotInit*/
    if (crv != CKR_OK) {
        sftk_fatalError = PR_TRUE;
        return crv;
    }

    sftk_fatalError = PR_FALSE; /* any error has been reset */
    nsf_init = PR_TRUE;
    isLevel2 = PR_TRUE; /* assume level 2 unless we learn otherwise */

    return CKR_OK;
}

/*FC_Finalize indicates that an application is done with the PKCS #11 library.*/
CK_RV
FC_Finalize(CK_VOID_PTR pReserved)
{
    CK_RV crv;

    if (sftk_ForkReset(pReserved, &crv)) {
        return crv;
    }

    if (!nsf_init) {
        return CKR_OK;
    }

    crv = nsc_CommonFinalize(pReserved, PR_TRUE);

    nsf_init = (PRBool) !(crv == CKR_OK);
    return crv;
}

/* FC_GetInfo returns general information about PKCS #11. */
CK_RV
FC_GetInfo(CK_INFO_PTR pInfo)
{
    CHECK_FORK();

    return NSC_GetInfo(pInfo);
}

/* FC_GetSlotList obtains a list of slots in the system. */
CK_RV
FC_GetSlotList(CK_BBOOL tokenPresent,
               CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount)
{
    CHECK_FORK();

    return nsc_CommonGetSlotList(tokenPresent, pSlotList, pulCount,
                                 NSC_FIPS_MODULE);
}

/* FC_GetSlotInfo obtains information about a particular slot in the system. */
CK_RV
FC_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
    CHECK_FORK();

    return NSC_GetSlotInfo(slotID, pInfo);
}

/*FC_GetTokenInfo obtains information about a particular token in the system.*/
CK_RV
FC_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_GetTokenInfo(slotID, pInfo);
    if (crv == CKR_OK) {
        if ((pInfo->flags & CKF_LOGIN_REQUIRED) == 0) {
            isLevel2 = PR_FALSE;
        }
    }
    return crv;
}

/*FC_GetMechanismList obtains a list of mechanism types supported by a token.*/
CK_RV
FC_GetMechanismList(CK_SLOT_ID slotID,
                    CK_MECHANISM_TYPE_PTR pMechanismList, CK_ULONG_PTR pusCount)
{
    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    if ((slotID == FIPS_SLOT_ID) || (slotID >= SFTK_MIN_FIPS_USER_SLOT_ID)) {
        slotID = NETSCAPE_SLOT_ID;
    }
    /* FIPS Slots support all functions */
    return NSC_GetMechanismList(slotID, pMechanismList, pusCount);
}

/* FC_GetMechanismInfo obtains information about a particular mechanism
 * possibly supported by a token. */
CK_RV
FC_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type,
                    CK_MECHANISM_INFO_PTR pInfo)
{
    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    if ((slotID == FIPS_SLOT_ID) || (slotID >= SFTK_MIN_FIPS_USER_SLOT_ID)) {
        slotID = NETSCAPE_SLOT_ID;
    }
    /* FIPS Slots support all functions */
    return NSC_GetMechanismInfo(slotID, type, pInfo);
}

/* FC_InitToken initializes a token. */
CK_RV
FC_InitToken(CK_SLOT_ID slotID, CK_CHAR_PTR pPin,
             CK_ULONG usPinLen, CK_CHAR_PTR pLabel)
{
    CK_RV crv;

    CHECK_FORK();

    crv = NSC_InitToken(slotID, pPin, usPinLen, pLabel);
    if (sftk_audit_enabled) {
        char msg[128];
        NSSAuditSeverity severity = (crv == CKR_OK) ? NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
        /* pLabel points to a 32-byte label, which is not null-terminated */
        PR_snprintf(msg, sizeof msg,
                    "C_InitToken(slotID=%lu, pLabel=\"%.32s\")=0x%08lX",
                    (PRUint32)slotID, pLabel, (PRUint32)crv);
        sftk_LogAuditMessage(severity, NSS_AUDIT_INIT_TOKEN, msg);
    }
    return crv;
}

/* FC_InitPIN initializes the normal user's PIN. */
CK_RV
FC_InitPIN(CK_SESSION_HANDLE hSession,
           CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    CK_RV rv;

    CHECK_FORK();

    if (sftk_fatalError)
        return CKR_DEVICE_ERROR;
    /* NSC_InitPIN will only work once per database. We can either initialize
     * it to level1 (pin len == 0) or level2. If we initialize to level 2, then
     * we need to make sure the pin meets FIPS requirements */
    if ((ulPinLen == 0) || ((rv = sftk_newPinCheck(pPin, ulPinLen)) == CKR_OK)) {
        rv = NSC_InitPIN(hSession, pPin, ulPinLen);
        if (rv == CKR_OK) {
            isLevel2 = (ulPinLen > 0) ? PR_TRUE : PR_FALSE;
        }
    }
    if (sftk_audit_enabled) {
        char msg[128];
        NSSAuditSeverity severity = (rv == CKR_OK) ? NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
        PR_snprintf(msg, sizeof msg,
                    "C_InitPIN(hSession=0x%08lX)=0x%08lX",
                    (PRUint32)hSession, (PRUint32)rv);
        sftk_LogAuditMessage(severity, NSS_AUDIT_INIT_PIN, msg);
    }
    return rv;
}

/* FC_SetPIN modifies the PIN of user that is currently logged in. */
/* NOTE: This is only valid for the PRIVATE_KEY_SLOT */
CK_RV
FC_SetPIN(CK_SESSION_HANDLE hSession, CK_CHAR_PTR pOldPin,
          CK_ULONG usOldLen, CK_CHAR_PTR pNewPin, CK_ULONG usNewLen)
{
    CK_RV rv;

    CHECK_FORK();

    if ((rv = sftk_fipsCheck()) == CKR_OK &&
        (rv = sftk_newPinCheck(pNewPin, usNewLen)) == CKR_OK) {
        rv = NSC_SetPIN(hSession, pOldPin, usOldLen, pNewPin, usNewLen);
        if (rv == CKR_OK) {
            /* if we set the password in level1 we now go
             * to level2. NOTE: we don't allow the user to
             * go from level2 to level1 */
            isLevel2 = PR_TRUE;
        }
    }
    if (sftk_audit_enabled) {
        char msg[128];
        NSSAuditSeverity severity = (rv == CKR_OK) ? NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
        PR_snprintf(msg, sizeof msg,
                    "C_SetPIN(hSession=0x%08lX)=0x%08lX",
                    (PRUint32)hSession, (PRUint32)rv);
        sftk_LogAuditMessage(severity, NSS_AUDIT_SET_PIN, msg);
    }
    return rv;
}

/* FC_OpenSession opens a session between an application and a token. */
CK_RV
FC_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
               CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession)
{
    SFTK_FIPSFATALCHECK();

    CHECK_FORK();

    return NSC_OpenSession(slotID, flags, pApplication, Notify, phSession);
}

/* FC_CloseSession closes a session between an application and a token. */
CK_RV
FC_CloseSession(CK_SESSION_HANDLE hSession)
{
    CHECK_FORK();

    return NSC_CloseSession(hSession);
}

/* FC_CloseAllSessions closes all sessions with a token. */
CK_RV
FC_CloseAllSessions(CK_SLOT_ID slotID)
{

    CHECK_FORK();

    return NSC_CloseAllSessions(slotID);
}

/* FC_GetSessionInfo obtains information about the session. */
CK_RV
FC_GetSessionInfo(CK_SESSION_HANDLE hSession,
                  CK_SESSION_INFO_PTR pInfo)
{
    CK_RV rv;
    SFTK_FIPSFATALCHECK();

    CHECK_FORK();

    rv = NSC_GetSessionInfo(hSession, pInfo);
    if (rv == CKR_OK) {
        if ((isLoggedIn) && (pInfo->state == CKS_RO_PUBLIC_SESSION)) {
            pInfo->state = CKS_RO_USER_FUNCTIONS;
        }
        if ((isLoggedIn) && (pInfo->state == CKS_RW_PUBLIC_SESSION)) {
            pInfo->state = CKS_RW_USER_FUNCTIONS;
        }
    }
    return rv;
}

/* FC_Login logs a user into a token. */
CK_RV
FC_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
         CK_CHAR_PTR pPin, CK_ULONG usPinLen)
{
    CK_RV rv;
    PRBool successful;
    if (sftk_fatalError)
        return CKR_DEVICE_ERROR;
    rv = NSC_Login(hSession, userType, pPin, usPinLen);
    successful = (rv == CKR_OK) || (rv == CKR_USER_ALREADY_LOGGED_IN);
    if (successful)
        isLoggedIn = PR_TRUE;
    if (sftk_audit_enabled) {
        char msg[128];
        NSSAuditSeverity severity;
        severity = successful ? NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
        PR_snprintf(msg, sizeof msg,
                    "C_Login(hSession=0x%08lX, userType=%lu)=0x%08lX",
                    (PRUint32)hSession, (PRUint32)userType, (PRUint32)rv);
        sftk_LogAuditMessage(severity, NSS_AUDIT_LOGIN, msg);
    }
    return rv;
}

/* FC_Logout logs a user out from a token. */
CK_RV
FC_Logout(CK_SESSION_HANDLE hSession)
{
    CK_RV rv;

    CHECK_FORK();

    if ((rv = sftk_fipsCheck()) == CKR_OK) {
        rv = NSC_Logout(hSession);
        isLoggedIn = PR_FALSE;
    }
    if (sftk_audit_enabled) {
        char msg[128];
        NSSAuditSeverity severity = (rv == CKR_OK) ? NSS_AUDIT_INFO : NSS_AUDIT_ERROR;
        PR_snprintf(msg, sizeof msg,
                    "C_Logout(hSession=0x%08lX)=0x%08lX",
                    (PRUint32)hSession, (PRUint32)rv);
        sftk_LogAuditMessage(severity, NSS_AUDIT_LOGOUT, msg);
    }
    return rv;
}

/* FC_CreateObject creates a new object. */
CK_RV
FC_CreateObject(CK_SESSION_HANDLE hSession,
                CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
                CK_OBJECT_HANDLE_PTR phObject)
{
    CK_OBJECT_CLASS *classptr;
    CK_RV rv = CKR_OK;

    CHECK_FORK();

    classptr = (CK_OBJECT_CLASS *)fc_getAttribute(pTemplate, ulCount, CKA_CLASS);
    if (classptr == NULL)
        return CKR_TEMPLATE_INCOMPLETE;

    if (*classptr == CKO_NETSCAPE_NEWSLOT || *classptr == CKO_NETSCAPE_DELSLOT) {
        if (sftk_fatalError)
            return CKR_DEVICE_ERROR;
    } else {
        rv = sftk_fipsCheck();
        if (rv != CKR_OK)
            return rv;
    }

    /* FIPS can't create keys from raw key material */
    if (SFTK_IS_NONPUBLIC_KEY_OBJECT(*classptr)) {
        rv = CKR_ATTRIBUTE_VALUE_INVALID;
    } else {
        rv = NSC_CreateObject(hSession, pTemplate, ulCount, phObject);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(*classptr)) {
        sftk_AuditCreateObject(hSession, pTemplate, ulCount, phObject, rv);
    }
    return rv;
}

/* FC_CopyObject copies an object, creating a new object for the copy. */
CK_RV
FC_CopyObject(CK_SESSION_HANDLE hSession,
              CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
              CK_OBJECT_HANDLE_PTR phNewObject)
{
    CK_RV rv;
    CK_OBJECT_CLASS objClass = CKO_NOT_A_KEY;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    rv = sftk_get_object_class_and_fipsCheck(hSession, hObject, &objClass);
    if (rv == CKR_OK) {
        rv = NSC_CopyObject(hSession, hObject, pTemplate, ulCount, phNewObject);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(objClass)) {
        sftk_AuditCopyObject(hSession,
                             hObject, pTemplate, ulCount, phNewObject, rv);
    }
    return rv;
}

/* FC_DestroyObject destroys an object. */
CK_RV
FC_DestroyObject(CK_SESSION_HANDLE hSession,
                 CK_OBJECT_HANDLE hObject)
{
    CK_RV rv;
    CK_OBJECT_CLASS objClass = CKO_NOT_A_KEY;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    rv = sftk_get_object_class_and_fipsCheck(hSession, hObject, &objClass);
    if (rv == CKR_OK) {
        rv = NSC_DestroyObject(hSession, hObject);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(objClass)) {
        sftk_AuditDestroyObject(hSession, hObject, rv);
    }
    return rv;
}

/* FC_GetObjectSize gets the size of an object in bytes. */
CK_RV
FC_GetObjectSize(CK_SESSION_HANDLE hSession,
                 CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize)
{
    CK_RV rv;
    CK_OBJECT_CLASS objClass = CKO_NOT_A_KEY;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    rv = sftk_get_object_class_and_fipsCheck(hSession, hObject, &objClass);
    if (rv == CKR_OK) {
        rv = NSC_GetObjectSize(hSession, hObject, pulSize);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(objClass)) {
        sftk_AuditGetObjectSize(hSession, hObject, pulSize, rv);
    }
    return rv;
}

/* FC_GetAttributeValue obtains the value of one or more object attributes. */
CK_RV
FC_GetAttributeValue(CK_SESSION_HANDLE hSession,
                     CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
    CK_RV rv;
    CK_OBJECT_CLASS objClass = CKO_NOT_A_KEY;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    rv = sftk_get_object_class_and_fipsCheck(hSession, hObject, &objClass);
    if (rv == CKR_OK) {
        rv = NSC_GetAttributeValue(hSession, hObject, pTemplate, ulCount);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(objClass)) {
        sftk_AuditGetAttributeValue(hSession, hObject, pTemplate, ulCount, rv);
    }
    return rv;
}

/* FC_SetAttributeValue modifies the value of one or more object attributes */
CK_RV
FC_SetAttributeValue(CK_SESSION_HANDLE hSession,
                     CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
    CK_RV rv;
    CK_OBJECT_CLASS objClass = CKO_NOT_A_KEY;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    rv = sftk_get_object_class_and_fipsCheck(hSession, hObject, &objClass);
    if (rv == CKR_OK) {
        rv = NSC_SetAttributeValue(hSession, hObject, pTemplate, ulCount);
    }
    if (sftk_audit_enabled && SFTK_IS_KEY_OBJECT(objClass)) {
        sftk_AuditSetAttributeValue(hSession, hObject, pTemplate, ulCount, rv);
    }
    return rv;
}

/* FC_FindObjectsInit initializes a search for token and session objects
 * that match a template. */
CK_RV
FC_FindObjectsInit(CK_SESSION_HANDLE hSession,
                   CK_ATTRIBUTE_PTR pTemplate, CK_ULONG usCount)
{
    /* let publically readable object be found */
    unsigned int i;
    CK_RV rv;
    PRBool needLogin = PR_FALSE;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();

    for (i = 0; i < usCount; i++) {
        CK_OBJECT_CLASS class;
        if (pTemplate[i].type != CKA_CLASS) {
            continue;
        }
        if (pTemplate[i].ulValueLen != sizeof(CK_OBJECT_CLASS)) {
            continue;
        }
        if (pTemplate[i].pValue == NULL) {
            continue;
        }
        class = *(CK_OBJECT_CLASS *)pTemplate[i].pValue;
        if ((class == CKO_PRIVATE_KEY) || (class == CKO_SECRET_KEY)) {
            needLogin = PR_TRUE;
            break;
        }
    }
    if (needLogin) {
        if ((rv = sftk_fipsCheck()) != CKR_OK)
            return rv;
    }
    return NSC_FindObjectsInit(hSession, pTemplate, usCount);
}

/* FC_FindObjects continues a search for token and session objects
 * that match a template, obtaining additional object handles. */
CK_RV
FC_FindObjects(CK_SESSION_HANDLE hSession,
               CK_OBJECT_HANDLE_PTR phObject, CK_ULONG usMaxObjectCount,
               CK_ULONG_PTR pusObjectCount)
{
    CHECK_FORK();

    /* let publically readable object be found */
    SFTK_FIPSFATALCHECK();
    return NSC_FindObjects(hSession, phObject, usMaxObjectCount,
                           pusObjectCount);
}

/*
 ************** Crypto Functions:     Encrypt ************************
 */

/* FC_EncryptInit initializes an encryption operation. */
CK_RV
FC_EncryptInit(CK_SESSION_HANDLE hSession,
               CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_EncryptInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("Encrypt", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_Encrypt encrypts single-part data. */
CK_RV
FC_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
           CK_ULONG usDataLen, CK_BYTE_PTR pEncryptedData,
           CK_ULONG_PTR pusEncryptedDataLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_Encrypt(hSession, pData, usDataLen, pEncryptedData,
                       pusEncryptedDataLen);
}

/* FC_EncryptUpdate continues a multiple-part encryption operation. */
CK_RV
FC_EncryptUpdate(CK_SESSION_HANDLE hSession,
                 CK_BYTE_PTR pPart, CK_ULONG usPartLen, CK_BYTE_PTR pEncryptedPart,
                 CK_ULONG_PTR pusEncryptedPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_EncryptUpdate(hSession, pPart, usPartLen, pEncryptedPart,
                             pusEncryptedPartLen);
}

/* FC_EncryptFinal finishes a multiple-part encryption operation. */
CK_RV
FC_EncryptFinal(CK_SESSION_HANDLE hSession,
                CK_BYTE_PTR pLastEncryptedPart, CK_ULONG_PTR pusLastEncryptedPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_EncryptFinal(hSession, pLastEncryptedPart,
                            pusLastEncryptedPartLen);
}

/*
 ************** Crypto Functions:     Decrypt ************************
 */

/* FC_DecryptInit initializes a decryption operation. */
CK_RV
FC_DecryptInit(CK_SESSION_HANDLE hSession,
               CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_DecryptInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("Decrypt", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_Decrypt decrypts encrypted data in a single part. */
CK_RV
FC_Decrypt(CK_SESSION_HANDLE hSession,
           CK_BYTE_PTR pEncryptedData, CK_ULONG usEncryptedDataLen, CK_BYTE_PTR pData,
           CK_ULONG_PTR pusDataLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_Decrypt(hSession, pEncryptedData, usEncryptedDataLen, pData,
                       pusDataLen);
}

/* FC_DecryptUpdate continues a multiple-part decryption operation. */
CK_RV
FC_DecryptUpdate(CK_SESSION_HANDLE hSession,
                 CK_BYTE_PTR pEncryptedPart, CK_ULONG usEncryptedPartLen,
                 CK_BYTE_PTR pPart, CK_ULONG_PTR pusPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_DecryptUpdate(hSession, pEncryptedPart, usEncryptedPartLen,
                             pPart, pusPartLen);
}

/* FC_DecryptFinal finishes a multiple-part decryption operation. */
CK_RV
FC_DecryptFinal(CK_SESSION_HANDLE hSession,
                CK_BYTE_PTR pLastPart, CK_ULONG_PTR pusLastPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_DecryptFinal(hSession, pLastPart, pusLastPartLen);
}

/*
 ************** Crypto Functions:     Digest (HASH)  ************************
 */

/* FC_DigestInit initializes a message-digesting operation. */
CK_RV
FC_DigestInit(CK_SESSION_HANDLE hSession,
              CK_MECHANISM_PTR pMechanism)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_DigestInit(hSession, pMechanism);
}

/* FC_Digest digests data in a single part. */
CK_RV
FC_Digest(CK_SESSION_HANDLE hSession,
          CK_BYTE_PTR pData, CK_ULONG usDataLen, CK_BYTE_PTR pDigest,
          CK_ULONG_PTR pusDigestLen)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_Digest(hSession, pData, usDataLen, pDigest, pusDigestLen);
}

/* FC_DigestUpdate continues a multiple-part message-digesting operation. */
CK_RV
FC_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                CK_ULONG usPartLen)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_DigestUpdate(hSession, pPart, usPartLen);
}

/* FC_DigestFinal finishes a multiple-part message-digesting operation. */
CK_RV
FC_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
               CK_ULONG_PTR pusDigestLen)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_DigestFinal(hSession, pDigest, pusDigestLen);
}

/*
 ************** Crypto Functions:     Sign  ************************
 */

/* FC_SignInit initializes a signature (private key encryption) operation,
 * where the signature is (will be) an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
FC_SignInit(CK_SESSION_HANDLE hSession,
            CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_SignInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("Sign", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_Sign signs (encrypts with private key) data in a single part,
 * where the signature is (will be) an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
FC_Sign(CK_SESSION_HANDLE hSession,
        CK_BYTE_PTR pData, CK_ULONG usDataLen, CK_BYTE_PTR pSignature,
        CK_ULONG_PTR pusSignatureLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_Sign(hSession, pData, usDataLen, pSignature, pusSignatureLen);
}

/* FC_SignUpdate continues a multiple-part signature operation,
 * where the signature is (will be) an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
FC_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
              CK_ULONG usPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_SignUpdate(hSession, pPart, usPartLen);
}

/* FC_SignFinal finishes a multiple-part signature operation,
 * returning the signature. */
CK_RV
FC_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
             CK_ULONG_PTR pusSignatureLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_SignFinal(hSession, pSignature, pusSignatureLen);
}

/*
 ************** Crypto Functions:     Sign Recover  ************************
 */
/* FC_SignRecoverInit initializes a signature operation,
 * where the (digest) data can be recovered from the signature.
 * E.g. encryption with the user's private key */
CK_RV
FC_SignRecoverInit(CK_SESSION_HANDLE hSession,
                   CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_SignRecoverInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("SignRecover", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_SignRecover signs data in a single operation
 * where the (digest) data can be recovered from the signature.
 * E.g. encryption with the user's private key */
CK_RV
FC_SignRecover(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
               CK_ULONG usDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pusSignatureLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_SignRecover(hSession, pData, usDataLen, pSignature, pusSignatureLen);
}

/*
 ************** Crypto Functions:     verify  ************************
 */

/* FC_VerifyInit initializes a verification operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature (e.g. DSA) */
CK_RV
FC_VerifyInit(CK_SESSION_HANDLE hSession,
              CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_VerifyInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("Verify", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_Verify verifies a signature in a single-part operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
FC_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
          CK_ULONG usDataLen, CK_BYTE_PTR pSignature, CK_ULONG usSignatureLen)
{
    /* make sure we're legal */
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_Verify(hSession, pData, usDataLen, pSignature, usSignatureLen);
}

/* FC_VerifyUpdate continues a multiple-part verification operation,
 * where the signature is an appendix to the data,
 * and plaintext cannot be recovered from the signature */
CK_RV
FC_VerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                CK_ULONG usPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_VerifyUpdate(hSession, pPart, usPartLen);
}

/* FC_VerifyFinal finishes a multiple-part verification operation,
 * checking the signature. */
CK_RV
FC_VerifyFinal(CK_SESSION_HANDLE hSession,
               CK_BYTE_PTR pSignature, CK_ULONG usSignatureLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_VerifyFinal(hSession, pSignature, usSignatureLen);
}

/*
 ************** Crypto Functions:     Verify  Recover ************************
 */

/* FC_VerifyRecoverInit initializes a signature verification operation,
 * where the data is recovered from the signature.
 * E.g. Decryption with the user's public key */
CK_RV
FC_VerifyRecoverInit(CK_SESSION_HANDLE hSession,
                     CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_VerifyRecoverInit(hSession, pMechanism, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditCryptInit("VerifyRecover", hSession, pMechanism, hKey, rv);
    }
    return rv;
}

/* FC_VerifyRecover verifies a signature in a single-part operation,
 * where the data is recovered from the signature.
 * E.g. Decryption with the user's public key */
CK_RV
FC_VerifyRecover(CK_SESSION_HANDLE hSession,
                 CK_BYTE_PTR pSignature, CK_ULONG usSignatureLen,
                 CK_BYTE_PTR pData, CK_ULONG_PTR pusDataLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_VerifyRecover(hSession, pSignature, usSignatureLen, pData,
                             pusDataLen);
}

/*
 **************************** Key Functions:  ************************
 */

/* FC_GenerateKey generates a secret key, creating a new key object. */
CK_RV
FC_GenerateKey(CK_SESSION_HANDLE hSession,
               CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
               CK_OBJECT_HANDLE_PTR phKey)
{
    CK_BBOOL *boolptr;

    SFTK_FIPSCHECK();
    CHECK_FORK();

    /* all secret keys must be sensitive, if the upper level code tries to say
     * otherwise, reject it. */
    boolptr = (CK_BBOOL *)fc_getAttribute(pTemplate, ulCount, CKA_SENSITIVE);
    if (boolptr != NULL) {
        if (!(*boolptr)) {
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
    }

    rv = NSC_GenerateKey(hSession, pMechanism, pTemplate, ulCount, phKey);
    if (sftk_audit_enabled) {
        sftk_AuditGenerateKey(hSession, pMechanism, pTemplate, ulCount, phKey, rv);
    }
    return rv;
}

/* FC_GenerateKeyPair generates a public-key/private-key pair,
 * creating new key objects. */
CK_RV
FC_GenerateKeyPair(CK_SESSION_HANDLE hSession,
                   CK_MECHANISM_PTR pMechanism, CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                   CK_ULONG usPublicKeyAttributeCount, CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                   CK_ULONG usPrivateKeyAttributeCount, CK_OBJECT_HANDLE_PTR phPublicKey,
                   CK_OBJECT_HANDLE_PTR phPrivateKey)
{
    CK_BBOOL *boolptr;
    CK_RV crv;

    SFTK_FIPSCHECK();
    CHECK_FORK();

    /* all private keys must be sensitive, if the upper level code tries to say
     * otherwise, reject it. */
    boolptr = (CK_BBOOL *)fc_getAttribute(pPrivateKeyTemplate,
                                          usPrivateKeyAttributeCount, CKA_SENSITIVE);
    if (boolptr != NULL) {
        if (!(*boolptr)) {
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
    }
    crv = NSC_GenerateKeyPair(hSession, pMechanism, pPublicKeyTemplate,
                              usPublicKeyAttributeCount, pPrivateKeyTemplate,
                              usPrivateKeyAttributeCount, phPublicKey, phPrivateKey);
    if (crv == CKR_GENERAL_ERROR) {
        /* pairwise consistency check failed. */
        sftk_fatalError = PR_TRUE;
    }
    if (sftk_audit_enabled) {
        sftk_AuditGenerateKeyPair(hSession, pMechanism, pPublicKeyTemplate,
                                  usPublicKeyAttributeCount, pPrivateKeyTemplate,
                                  usPrivateKeyAttributeCount, phPublicKey, phPrivateKey, crv);
    }
    return crv;
}

/* FC_WrapKey wraps (i.e., encrypts) a key. */
CK_RV
FC_WrapKey(CK_SESSION_HANDLE hSession,
           CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hWrappingKey,
           CK_OBJECT_HANDLE hKey, CK_BYTE_PTR pWrappedKey,
           CK_ULONG_PTR pulWrappedKeyLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_WrapKey(hSession, pMechanism, hWrappingKey, hKey, pWrappedKey,
                     pulWrappedKeyLen);
    if (sftk_audit_enabled) {
        sftk_AuditWrapKey(hSession, pMechanism, hWrappingKey, hKey, pWrappedKey,
                          pulWrappedKeyLen, rv);
    }
    return rv;
}

/* FC_UnwrapKey unwraps (decrypts) a wrapped key, creating a new key object. */
CK_RV
FC_UnwrapKey(CK_SESSION_HANDLE hSession,
             CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hUnwrappingKey,
             CK_BYTE_PTR pWrappedKey, CK_ULONG ulWrappedKeyLen,
             CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
             CK_OBJECT_HANDLE_PTR phKey)
{
    CK_BBOOL *boolptr;

    SFTK_FIPSCHECK();
    CHECK_FORK();

    /* all secret keys must be sensitive, if the upper level code tries to say
     * otherwise, reject it. */
    boolptr = (CK_BBOOL *)fc_getAttribute(pTemplate,
                                          ulAttributeCount, CKA_SENSITIVE);
    if (boolptr != NULL) {
        if (!(*boolptr)) {
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
    }
    rv = NSC_UnwrapKey(hSession, pMechanism, hUnwrappingKey, pWrappedKey,
                       ulWrappedKeyLen, pTemplate, ulAttributeCount, phKey);
    if (sftk_audit_enabled) {
        sftk_AuditUnwrapKey(hSession, pMechanism, hUnwrappingKey, pWrappedKey,
                            ulWrappedKeyLen, pTemplate, ulAttributeCount, phKey, rv);
    }
    return rv;
}

/* FC_DeriveKey derives a key from a base key, creating a new key object. */
CK_RV
FC_DeriveKey(CK_SESSION_HANDLE hSession,
             CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hBaseKey,
             CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulAttributeCount,
             CK_OBJECT_HANDLE_PTR phKey)
{
    CK_BBOOL *boolptr;

    SFTK_FIPSCHECK();
    CHECK_FORK();

    /* all secret keys must be sensitive, if the upper level code tries to say
     * otherwise, reject it. */
    boolptr = (CK_BBOOL *)fc_getAttribute(pTemplate,
                                          ulAttributeCount, CKA_SENSITIVE);
    if (boolptr != NULL) {
        if (!(*boolptr)) {
            return CKR_ATTRIBUTE_VALUE_INVALID;
        }
    }
    rv = NSC_DeriveKey(hSession, pMechanism, hBaseKey, pTemplate,
                       ulAttributeCount, phKey);
    if (sftk_audit_enabled) {
        sftk_AuditDeriveKey(hSession, pMechanism, hBaseKey, pTemplate,
                            ulAttributeCount, phKey, rv);
    }
    return rv;
}

/*
 **************************** Radom Functions:  ************************
 */

/* FC_SeedRandom mixes additional seed material into the token's random number
 * generator. */
CK_RV
FC_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed,
              CK_ULONG usSeedLen)
{
    CK_RV crv;

    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    crv = NSC_SeedRandom(hSession, pSeed, usSeedLen);
    if (crv != CKR_OK) {
        sftk_fatalError = PR_TRUE;
    }
    return crv;
}

/* FC_GenerateRandom generates random data. */
CK_RV
FC_GenerateRandom(CK_SESSION_HANDLE hSession,
                  CK_BYTE_PTR pRandomData, CK_ULONG ulRandomLen)
{
    CK_RV crv;

    CHECK_FORK();

    SFTK_FIPSFATALCHECK();
    crv = NSC_GenerateRandom(hSession, pRandomData, ulRandomLen);
    if (crv != CKR_OK) {
        sftk_fatalError = PR_TRUE;
        if (sftk_audit_enabled) {
            char msg[128];
            PR_snprintf(msg, sizeof msg,
                        "C_GenerateRandom(hSession=0x%08lX, pRandomData=%p, "
                        "ulRandomLen=%lu)=0x%08lX "
                        "self-test: continuous RNG test failed",
                        (PRUint32)hSession, pRandomData,
                        (PRUint32)ulRandomLen, (PRUint32)crv);
            sftk_LogAuditMessage(NSS_AUDIT_ERROR, NSS_AUDIT_SELF_TEST, msg);
        }
    }
    return crv;
}

/* FC_GetFunctionStatus obtains an updated status of a function running
 * in parallel with an application. */
CK_RV
FC_GetFunctionStatus(CK_SESSION_HANDLE hSession)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_GetFunctionStatus(hSession);
}

/* FC_CancelFunction cancels a function running in parallel */
CK_RV
FC_CancelFunction(CK_SESSION_HANDLE hSession)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_CancelFunction(hSession);
}

/*
 ****************************  Version 1.1 Functions:  ************************
 */

/* FC_GetOperationState saves the state of the cryptographic
 *operation in a session. */
CK_RV
FC_GetOperationState(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pOperationState, CK_ULONG_PTR pulOperationStateLen)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_GetOperationState(hSession, pOperationState, pulOperationStateLen);
}

/* FC_SetOperationState restores the state of the cryptographic operation
 * in a session. */
CK_RV
FC_SetOperationState(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pOperationState, CK_ULONG ulOperationStateLen,
                     CK_OBJECT_HANDLE hEncryptionKey, CK_OBJECT_HANDLE hAuthenticationKey)
{
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_SetOperationState(hSession, pOperationState, ulOperationStateLen,
                                 hEncryptionKey, hAuthenticationKey);
}

/* FC_FindObjectsFinal finishes a search for token and session objects. */
CK_RV
FC_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
    /* let publically readable object be found */
    SFTK_FIPSFATALCHECK();
    CHECK_FORK();

    return NSC_FindObjectsFinal(hSession);
}

/* Dual-function cryptographic operations */

/* FC_DigestEncryptUpdate continues a multiple-part digesting and encryption
 * operation. */
CK_RV
FC_DigestEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                       CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,
                       CK_ULONG_PTR pulEncryptedPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_DigestEncryptUpdate(hSession, pPart, ulPartLen, pEncryptedPart,
                                   pulEncryptedPartLen);
}

/* FC_DecryptDigestUpdate continues a multiple-part decryption and digesting
 * operation. */
CK_RV
FC_DecryptDigestUpdate(CK_SESSION_HANDLE hSession,
                       CK_BYTE_PTR pEncryptedPart, CK_ULONG ulEncryptedPartLen,
                       CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_DecryptDigestUpdate(hSession, pEncryptedPart, ulEncryptedPartLen,
                                   pPart, pulPartLen);
}

/* FC_SignEncryptUpdate continues a multiple-part signing and encryption
 * operation. */
CK_RV
FC_SignEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                     CK_ULONG ulPartLen, CK_BYTE_PTR pEncryptedPart,
                     CK_ULONG_PTR pulEncryptedPartLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_SignEncryptUpdate(hSession, pPart, ulPartLen, pEncryptedPart,
                                 pulEncryptedPartLen);
}

/* FC_DecryptVerifyUpdate continues a multiple-part decryption and verify
 * operation. */
CK_RV
FC_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession,
                       CK_BYTE_PTR pEncryptedData, CK_ULONG ulEncryptedDataLen,
                       CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    return NSC_DecryptVerifyUpdate(hSession, pEncryptedData, ulEncryptedDataLen,
                                   pData, pulDataLen);
}

/* FC_DigestKey continues a multi-part message-digesting operation,
 * by digesting the value of a secret key as part of the data already digested.
 */
CK_RV
FC_DigestKey(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hKey)
{
    SFTK_FIPSCHECK();
    CHECK_FORK();

    rv = NSC_DigestKey(hSession, hKey);
    if (sftk_audit_enabled) {
        sftk_AuditDigestKey(hSession, hKey, rv);
    }
    return rv;
}

CK_RV
FC_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot,
                    CK_VOID_PTR pReserved)
{
    CHECK_FORK();

    return NSC_WaitForSlotEvent(flags, pSlot, pReserved);
}
