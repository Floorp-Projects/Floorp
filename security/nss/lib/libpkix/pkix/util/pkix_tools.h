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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
/*
 * pkix_tools.h
 *
 * Header for Utility Functions and Macros
 *
 */

#ifndef _PKIX_TOOLS_H
#define _PKIX_TOOLS_H

#include "pkix.h"
#include <stddef.h>
#include <stdio.h>

/* private PKIX system headers */
#include "pkix_basicconstraintschecker.h"
#include "pkix_buildresult.h"
#include "pkix_certchainchecker.h"
#include "pkix_certselector.h"
#include "pkix_comcertselparams.h"
#include "pkix_comcrlselparams.h"
#include "pkix_crlselector.h"
#include "pkix_defaultcrlchecker.h"
#include "pkix_defaultrevchecker.h"
#include "pkix_error.h"
#include "pkix_expirationchecker.h"
#include "pkix_list.h"
#include "pkix_logger.h"
#include "pkix_namechainingchecker.h"
#include "pkix_nameconstraintschecker.h"
#include "pkix_ocspchecker.h"
#include "pkix_policychecker.h"
#include "pkix_policynode.h"
#include "pkix_procparams.h"
#include "pkix_resourcelimits.h"
#include "pkix_revocationchecker.h"
#include "pkix_signaturechecker.h"
#include "pkix_store.h"
#include "pkix_targetcertchecker.h"
#include "pkix_validate.h"
#include "pkix_valresult.h"
#include "pkix_verifynode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UTILITY MACROS
 * Documentation for these common utility macros can be found in the
 * Implementation Guidelines document (Section 4.3)
 *
 * In general, macros with multiple statements (or a single "if" statement)
 * use the "do {<body>} while (0)" technique in order to convert the multiple
 * statements into one statement, thus avoiding the dangling else problem.
 * For macros which ALWAYS exit with a "return" or "goto", there is no
 * need to use this technique (and it yields compiler warnings of "statement
 * not reached"), so we just use "{<body>}" to group the statements together.
 */

#define PKIX_ENTER(type, funcName) \
        PKIX_STD_VARS(funcName); \
        pkixType = PKIX_ ## type ## _ERROR; \
        PKIX_DEBUG_ENTER(type); \
        do { \
            if (pkixLoggersDebugTrace) { \
                (pkix_Logger_Check(pkixLoggersDebugTrace, \
                        funcName, ">>>", pkixType, \
                        PKIX_LOGGER_LEVEL_TRACE, plContext)); \
            } \
        } while (0)

#define PKIX_ENTER_NO_LOGGER(type, funcName) \
        PKIX_STD_VARS(funcName); \
        pkixType = PKIX_ ## type ## _ERROR; \
        PKIX_DEBUG_ENTER(type);

#define PKIX_STD_VARS(funcName) \
        PKIX_Error *pkixErrorResult = NULL;     \
        PKIX_Error *pkixTempResult = NULL; \
        PKIX_Error *pkixReturnResult = NULL; \
        PKIX_ERRSTRINGNUM pkixErrMsgNum = 0; \
        char *pkixErrorMsg = NULL; \
        PKIX_Boolean pkixErrorReceived = PKIX_FALSE; \
        PKIX_Boolean pkixTempErrorReceived = PKIX_FALSE; \
        /* ARGSUSED */ PKIX_UInt32 pkixErrorCode = 0; \
        char *myFuncName = (funcName); \
        PKIX_UInt32 pkixType = 0; \
        PKIX_Boolean objectIsLocked = PKIX_FALSE; \
        PKIX_PL_Object *lockedObject = NULL; \
        PKIX_Boolean mutexIsLocked = PKIX_FALSE; \
        PKIX_PL_Mutex *lockedMutex = NULL;

#define PKIX_DEBUG_ENTER(type) \
        PKIX_ ## type ## _DEBUG_ARG("( Entering %s).\n", myFuncName)

#define PKIX_NULLCHECK_ONE(a) \
        do { \
                if ((a) == NULL){ \
                        PKIX_ERROR_FATAL(PKIX_NULLARGUMENT); \
                } \
        } while (0)

#define PKIX_NULLCHECK_TWO(a, b) \
        do { \
                if (((a) == NULL) || ((b) == NULL)){ \
                        PKIX_ERROR_FATAL(PKIX_NULLARGUMENT); \
                } \
        } while (0)

#define PKIX_NULLCHECK_THREE(a, b, c) \
        do { \
                if (((a) == NULL) || ((b) == NULL) || ((c) == NULL)){ \
                        PKIX_ERROR_FATAL(PKIX_NULLARGUMENT); \
                } \
        } while (0)

#define PKIX_NULLCHECK_FOUR(a, b, c, d) \
        do { \
                if (((a) == NULL) || ((b) == NULL) || \
                    ((c) == NULL) || ((d) == NULL)){ \
                        PKIX_ERROR_FATAL(PKIX_NULLARGUMENT); \
                } \
        } while (0)

#define PKIX_CHECK(func, descNum) \
        do { \
                pkixErrorResult = (func); \
                if (pkixErrorResult) { \
                        pkixTempResult = PKIX_Error_GetErrorCode \
                                (pkixErrorResult, &pkixErrorCode, plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                        if (pkixErrorCode == PKIX_FATAL_ERROR){ \
                                pkixErrorMsg = PKIX_ErrorText[descNum]; \
                                PKIX_RETURN(FATAL); \
                        } else { \
                                pkixErrorMsg = PKIX_ErrorText[descNum]; \
                                goto cleanup; \
                        } \
                } \
        } while (0)

#define PKIX_CHECK_ONLY_FATAL(func, descNum) \
        do { \
                pkixTempErrorReceived = PKIX_FALSE; \
                pkixErrorResult = (func); \
                if (pkixErrorResult) { \
                        pkixTempErrorReceived = PKIX_TRUE; \
                        pkixTempResult = PKIX_Error_GetErrorCode \
                                (pkixErrorResult, &pkixErrorCode, plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                        if (pkixErrorCode == PKIX_FATAL_ERROR){ \
                                pkixErrorMsg = PKIX_ErrorText[descNum]; \
                                PKIX_RETURN(FATAL); \
                        } \
                        PKIX_DECREF(pkixErrorResult); \
                } \
        } while (0)

#define PKIX_CHECK_FATAL(func, descNum) \
        do { \
                pkixTempResult = (func); \
                if (pkixTempResult) { \
                        PKIX_ERROR_FATAL(descNum); \
                } \
        } while (0)

#define PKIX_LOG_ERROR(descNum) \
        { \
                if (pkixLoggersErrors) { \
                    (pkix_Logger_Check(pkixLoggersErrors, \
                        PKIX_ErrorText[descNum], NULL, pkixType, \
                        PKIX_LOGGER_LEVEL_ERROR, plContext)); \
                } \
        }

#define PKIX_ERROR(descNum) \
        { \
                PKIX_LOG_ERROR(descNum) \
                pkixErrorReceived = PKIX_TRUE; \
                pkixErrorMsg = PKIX_ErrorText[descNum]; \
                goto cleanup; \
        }

#define PKIX_ERROR_FATAL(descNum) \
        { \
                pkixErrorReceived = PKIX_TRUE; \
                pkixErrorMsg = PKIX_ErrorText[descNum]; \
                if (pkixLoggersErrors) { \
                    (pkix_Logger_Check(pkixLoggersErrors, \
                        pkixErrorMsg, NULL, pkixType, \
                        PKIX_LOGGER_LEVEL_FATALERROR, plContext)); \
                } \
                PKIX_RETURN(FATAL); \
        }

#define PKIX_OBJECT_LOCK(obj) \
        do { \
                if (obj){ \
                        PKIX_CHECK(PKIX_PL_Object_Lock \
                                ((PKIX_PL_Object*)(obj), plContext), \
                                PKIX_OBJECTLOCKFAILED); \
                        objectIsLocked = PKIX_TRUE; \
                        lockedObject = (PKIX_PL_Object *)(obj); \
                } \
        } while (0)

#define PKIX_OBJECT_UNLOCK(obj) \
        do { \
                if (obj){ \
                        pkixTempResult = \
                                PKIX_PL_Object_Unlock \
                                ((PKIX_PL_Object *)(obj), plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                        objectIsLocked = PKIX_FALSE; \
                        lockedObject = NULL; \
                } \
        } while (0)

#define PKIX_MUTEX_LOCK(obj) \
        do { \
                if (obj){ \
                        PKIX_CHECK(PKIX_PL_Mutex_Lock((obj), plContext), \
                                PKIX_MUTEXLOCKFAILED); \
                        mutexIsLocked = PKIX_TRUE; \
                        lockedMutex = (obj); \
                } \
        } while (0)

#define PKIX_MUTEX_UNLOCK(obj) \
        do { \
                if (obj){ \
                        pkixTempResult = \
                                PKIX_PL_Mutex_Unlock((obj), plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                        mutexIsLocked = PKIX_FALSE; \
                        lockedMutex = NULL; \
                } \
        } while (0)


#define PKIX_RETURN(type) \
        { \
                if (objectIsLocked){ \
                        PKIX_OBJECT_UNLOCK(lockedObject); \
                } \
                if (mutexIsLocked){ \
                        PKIX_MUTEX_UNLOCK(lockedMutex); \
                } \
                if ((pkixErrorReceived) || (pkixErrorResult)){ \
                        PKIX_THROW(type, pkixErrorMsg); \
                } \
                PKIX_DEBUG_EXIT(type); \
                do { \
                    if (pkixLoggersDebugTrace) { \
                        (pkix_Logger_Check(pkixLoggersDebugTrace, \
                            myFuncName, "<<<", \
                            pkixType, PKIX_LOGGER_LEVEL_TRACE, plContext)); \
                    } \
                } while (0); \
                pkixErrorCode = 0; \
                return (NULL); \
        }

#define PKIX_RETURN_NO_LOGGER(type) \
        { \
                if (objectIsLocked){ \
                        PKIX_OBJECT_UNLOCK(lockedObject); \
                } \
                if (mutexIsLocked){ \
                        PKIX_MUTEX_UNLOCK(lockedMutex); \
                } \
                if ((pkixErrorReceived) || (pkixErrorResult)){ \
                        PKIX_THROW(type, pkixErrorMsg); \
                } \
                PKIX_DEBUG_EXIT(type); \
                pkixErrorCode = 0; \
                return (NULL); \
        }

#define PKIX_THROW(type, desc) \
        { \
                pkixTempResult = (PKIX_Error*)pkix_Throw \
                        (PKIX_ ## type ## _ERROR, myFuncName, desc, \
                        pkixErrorResult, &pkixReturnResult, plContext); \
                if (pkixErrorResult != PKIX_ALLOC_ERROR()){ \
                        PKIX_DECREF(pkixErrorResult); \
                } \
                if (pkixTempResult) return (pkixTempResult); \
                else return (pkixReturnResult); \
        }

#define PKIX_ERROR_CREATE(type, descNum, error) \
	{ \
                pkixTempResult = (PKIX_Error*)pkix_Throw \
                        (PKIX_ ## type ## _ERROR,  myFuncName, \
                        PKIX_ErrorText[descNum], NULL, &error, plContext); \
                if (pkixTempResult) error = pkixTempResult; \
        }
		

#define PKIX_ERROR_RECEIVED (pkixErrorReceived || pkixErrorResult ||\
                        pkixTempErrorReceived)

#define PKIX_DEBUG_EXIT(type) \
        PKIX_ ## type ## _DEBUG_ARG("( Exiting %s).\n", myFuncName)

#define PKIX_DECREF(obj) \
        do { \
                if (obj){ \
                        pkixTempResult = PKIX_PL_Object_DecRef \
                        ((PKIX_PL_Object *)(obj), plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                        obj = NULL; \
                } \
        } while (0)

#define PKIX_INCREF(obj) \
        do { \
                if (obj){ \
                        pkixTempResult = PKIX_PL_Object_IncRef \
                        ((PKIX_PL_Object *)(obj), plContext); \
                        if (pkixTempResult) return pkixTempResult; \
                } \
        } while (0)

#define PKIX_FREE(obj) \
        do { \
                if (obj) { \
                        pkixTempResult = PKIX_PL_Free((obj), plContext); \
                        obj = NULL; \
                } \
        } while (0)

#define PKIX_EXACTLY_ONE_NULL(a, b) (((a) && !(b)) || ((b) && !(a)))

/* DIGIT MACROS */

#define PKIX_ISDIGIT(c) (((c) >= '0')&&((c) <= '9'))

#define PKIX_ISXDIGIT(c) \
        (PKIX_ISDIGIT(c)||\
        (((c) >= 'a')&&((c) <= 'f'))||\
        (((c) >= 'A')&&((c) <= 'F')))

#define PKIX_TOSTRING(a, b, c, d) \
        do { \
                if ((a) != NULL) { \
                    PKIX_CHECK(PKIX_PL_Object_ToString\
                        ((PKIX_PL_Object *)(a), (b), (c)), (d)); \
                } else { \
                    PKIX_CHECK(PKIX_PL_String_Create(PKIX_ESCASCII, "(null)", \
                        0, (b), (c)), PKIX_STRINGCREATEFAILED); \
                } \
        } while (0)

#define PKIX_EQUALS(a, b, c, d, e) \
        do { \
                if ((a) != NULL && (b) != NULL) { \
                    PKIX_CHECK(PKIX_PL_Object_Equals\
                        ((PKIX_PL_Object *)(a), \
                        (PKIX_PL_Object*)(b), \
                        (c), \
                        (d)), \
                        (e)); \
                } else { \
                    if ((a) == NULL && (b) == NULL) { \
                        *(c) = PKIX_TRUE; \
                    } else { \
                        *(c) = PKIX_FALSE; \
                    } \
                } \
        } while (0)

#define PKIX_HASHCODE(a, b, c, d) \
        do { \
                if ((a) != NULL) { \
                    PKIX_CHECK(PKIX_PL_Object_Hashcode\
                        ((PKIX_PL_Object *)(a), (b), (c)), (d)); \
                } else { \
                    *(b) = 0; \
                } \
        } while (0)

#define PKIX_DUPLICATE(a, b, c, d) \
        do { \
                if ((a) != NULL) { \
                    PKIX_CHECK(PKIX_PL_Object_Duplicate\
                        ((PKIX_PL_Object *)(a), \
                        (PKIX_PL_Object **)(b), \
                        (c)), \
                        (d)); \
                } else { \
                    *(b) = (a); \
                } \
        } while (0)

/*
 * DEBUG MACROS
 *
 * Each type has an associated debug flag, which can
 * be set on the compiler line using "-D<debugflag>". For convenience,
 * "-DPKIX_DEBUGALL" turns on debug for all the components.
 *
 * If a type's debug flag is defined, then its two associated macros
 * are defined: PKIX_type_DEBUG(expr) and PKIX_type_DEBUG_ARG(expr, arg),
 * which call PKIX_DEBUG(expr) and PKIX_DEBUG_ARG(expr, arg) respectively,
 * which, in turn, enable standard and consistently formatted output.
 *
 * If a type's debug flag is not defined, the two associated macros
 * are defined as a NO-OP. As such, any PKIX_type_DEBUG or PKIX_type_DEBUG_ARG
 * macros for an undefined type will be stripped from the code during
 * pre-processing, thereby reducing code size.
 */

#ifdef PKIX_DEBUGALL
#define PKIX_REFCOUNTDEBUG                        1
#define PKIX_MEMDEBUG                             1
#define PKIX_MUTEXDEBUG                           1
#define PKIX_OBJECTDEBUG                          1
#define PKIX_STRINGDEBUG                          1
#define PKIX_OIDDEBUG                             1
#define PKIX_LISTDEBUG                            1
#define PKIX_ERRORDEBUG                           1
#define PKIX_BYTEARRAYDEBUG                       1
#define PKIX_RWLOCKDEBUG                          1
#define PKIX_BIGINTDEBUG                          1
#define PKIX_HASHTABLEDEBUG                       1
#define PKIX_X500NAMEDEBUG                        1
#define PKIX_GENERALNAMEDEBUG                     1
#define PKIX_PUBLICKEYDEBUG                       1
#define PKIX_CERTDEBUG                            1
#define PKIX_HTTPCLIENTDEBUG                      1
#define PKIX_DATEDEBUG                            1
#define PKIX_TRUSTANCHORDEBUG                     1
#define PKIX_PROCESSINGPARAMSDEBUG                1
#define PKIX_VALIDATEPARAMSDEBUG                  1
#define PKIX_VALIDATERESULTDEBUG                  1
#define PKIX_VALIDATEDEBUG                        1
#define PKIX_CERTCHAINCHECKERDEBUG                1
#define PKIX_REVOCATIONCHECKERDEBUG               1
#define PKIX_CERTSELECTORDEBUG                    1
#define PKIX_COMCERTSELPARAMSDEBUG                1
#define PKIX_TARGETCERTCHECKERSTATEDEBUG          1
#define PKIX_INITIALIZEPARAMSDEBUG                1
#define PKIX_CERTBASICCONSTRAINTSDEBUG            1
#define PKIX_CERTNAMECONSTRAINTSDEBUG             1
#define PKIX_CERTNAMECONSTRAINTSCHECKERSTATEDEBUG 1
#define PKIX_SUBJALTNAMECHECKERSTATEDEBUG         1

#define PKIX_CERTPOLICYQUALIFIERDEBUG             1
#define PKIX_CERTPOLICYINFODEBUG                  1
#define PKIX_CERTPOLICYNODEDEBUG                  1
#define PKIX_CERTPOLICYCHECKERSTATEDEBUG          1
#define PKIX_LIFECYCLEDEBUG                       1
#define PKIX_BASICCONSTRAINTSCHECKERSTATEDEBUG    1
#define PKIX_CRLDEBUG                             1
#define PKIX_CRLENTRYDEBUG                        1
#define PKIX_CRLSELECTORDEBUG                     1
#define PKIX_COMCRLSELPARAMSDEBUG                 1
#define PKIX_CERTSTOREDEBUG                       1
#define PKIX_COLLECTIONCERTSTORECONTEXTDEBUG      1
#define PKIX_DEFAULTCRLCHECKERSTATEDEBUG          1
#define PKIX_CERTPOLICYMAPDEBUG                   1
#define PKIX_BUILDDEBUG                           1
#define PKIX_BUILDRESULTDEBUG                     1
#define PKIX_FORWARDBUILDERSTATEDEBUG             1
#define PKIX_SIGNATURECHECKERSTATEDEBUG           1
#define PKIX_USERDEFINEDMODULESDEBUG              1
#define PKIX_CONTEXTDEBUG                         1
#define PKIX_DEFAULTREVOCATIONCHECKERDEBUG        1
#define PKIX_LDAPREQUESTDEBUG                     1
#define PKIX_LDAPRESPONSEDEBUG                    1
#define PKIX_LDAPCLIENTDEBUG                      1
#define PKIX_LDAPDEFAULTCLIENTDEBUG               1
#define PKIX_SOCKETDEBUG                          1
#define PKIX_RESOURCELIMITSDEBUG                  1
#define PKIX_LOGGERDEBUG                          1
#define PKIX_MONITORLOCKDEBUG                     1
#define PKIX_INFOACCESSDEBUG                      1
#define PKIX_AIAMGRDEBUG                          1
#define PKIX_OCSPCHECKERDEBUG                     1
#define PKIX_OCSPREQUESTDEBUG                     1
#define PKIX_OCSPRESPONSEDEBUG                    1
#define PKIX_HTTPDEFAULTCLIENTDEBUG               1
#define PKIX_HTTPCERTSTORECONTEXTDEBUG            1
#define PKIX_VERIFYNODEDEBUG                      1
#endif

/*
 * XXX Both PKIX_DEBUG and PKIX_DEBUG_ARG currently use printf.
 * This needs to be replaced with Loggers.
 */

#define PKIX_DEBUG(expr) \
        do { \
                if (pkixLoggersErrors) { \
                     (pkix_Logger_Check(pkixLoggersDebugTrace, \
                                myFuncName, expr, pkixType, \
                                PKIX_LOGGER_LEVEL_DEBUG, plContext)); \
                } \
                (void) printf("(%s: ", myFuncName); \
                (void) printf(expr); \
        } while (0)

/* Logging doesn't support DEBUG with ARG: cannot convert control and arg */
#define PKIX_DEBUG_ARG(expr, arg) \
        do { \
                (void) printf("(%s: ", myFuncName); \
                (void) printf(expr, arg); \
        } while (0)

#if PKIX_FATALDEBUG
#define PKIX_FATAL_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_FATAL_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_FATAL_DEBUG(expr)
#define PKIX_FATAL_DEBUG_ARG(expr, arg)
#endif

#if PKIX_REFCOUNTDEBUG
#define PKIX_REF_COUNT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_REF_COUNT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_REF_COUNT_DEBUG(expr)
#define PKIX_REF_COUNT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_MEMDEBUG
#define PKIX_MEM_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_MEM_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_MEM_DEBUG(expr)
#define PKIX_MEM_DEBUG_ARG(expr, arg)
#endif

#if PKIX_MUTEXDEBUG
#define PKIX_MUTEX_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_MUTEX_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_MUTEX_DEBUG(expr)
#define PKIX_MUTEX_DEBUG_ARG(expr, arg)
#endif

#if PKIX_OBJECTDEBUG
#define PKIX_OBJECT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_OBJECT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_OBJECT_DEBUG(expr)
#define PKIX_OBJECT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_ERRORDEBUG
#define PKIX_ERROR_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_ERROR_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_ERROR_DEBUG(expr)
#define PKIX_ERROR_DEBUG_ARG(expr, arg)
#endif

#if PKIX_STRINGDEBUG
#define PKIX_STRING_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_STRING_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_STRING_DEBUG(expr)
#define PKIX_STRING_DEBUG_ARG(expr, arg)
#endif

#if PKIX_OIDDEBUG
#define PKIX_OID_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_OID_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_OID_DEBUG(expr)
#define PKIX_OID_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LISTDEBUG
#define PKIX_LIST_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LIST_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LIST_DEBUG(expr)
#define PKIX_LIST_DEBUG_ARG(expr, arg)
#endif

#if PKIX_RWLOCKDEBUG
#define PKIX_RWLOCK_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_RWLOCK_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_RWLOCK_DEBUG(expr)
#define PKIX_RWLOCK_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BYTEARRAYDEBUG
#define PKIX_BYTEARRAY_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BYTEARRAY_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BYTEARRAY_DEBUG(expr)
#define PKIX_BYTEARRAY_DEBUG_ARG(expr, arg)
#endif

#if PKIX_HASHTABLEDEBUG
#define PKIX_HASHTABLE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_HASHTABLE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_HASHTABLE_DEBUG(expr)
#define PKIX_HASHTABLE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_X500NAMEDEBUG
#define PKIX_X500NAME_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_X500NAME_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_X500NAME_DEBUG(expr)
#define PKIX_X500NAME_DEBUG_ARG(expr, arg)
#endif

#if PKIX_GENERALNAMEDEBUG
#define PKIX_GENERALNAME_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_GENERALNAME_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_GENERALNAME_DEBUG(expr)
#define PKIX_GENERALNAME_DEBUG_ARG(expr, arg)
#endif

#if PKIX_PUBLICKEYDEBUG
#define PKIX_PUBLICKEY_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_PUBLICKEY_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_PUBLICKEY_DEBUG(expr)
#define PKIX_PUBLICKEY_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTDEBUG
#define PKIX_CERT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERT_DEBUG(expr)
#define PKIX_CERT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_HTTPCLIENTDEBUG
#define PKIX_HTTPCLIENT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_HTTPCLIENT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_HTTPCLIENT_DEBUG(expr)
#define PKIX_HTTPCLIENT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BIGINTDEBUG
#define PKIX_BIGINT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BIGINT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BIGINT_DEBUG(expr)
#define PKIX_BIGINT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_DATEDEBUG
#define PKIX_DATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_DATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_DATE_DEBUG(expr)
#define PKIX_DATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_TRUSTANCHORDEBUG
#define PKIX_TRUSTANCHOR_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_TRUSTANCHOR_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_TRUSTANCHOR_DEBUG(expr)
#define PKIX_TRUSTANCHOR_DEBUG_ARG(expr, arg)
#endif

#if PKIX_PROCESSINGPARAMSDEBUG
#define PKIX_PROCESSINGPARAMS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_PROCESSINGPARAMS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_PROCESSINGPARAMS_DEBUG(expr)
#define PKIX_PROCESSINGPARAMS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_VALIDATEPARAMSDEBUG
#define PKIX_VALIDATEPARAMS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_VALIDATEPARAMS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_VALIDATEPARAMS_DEBUG(expr)
#define PKIX_VALIDATEPARAMS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_VALIDATERESULTDEBUG
#define PKIX_VALIDATERESULT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_VALIDATERESULT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_VALIDATERESULT_DEBUG(expr)
#define PKIX_VALIDATERESULT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_VALIDATEDEBUG
#define PKIX_VALIDATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_VALIDATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_VALIDATE_DEBUG(expr)
#define PKIX_VALIDATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BUILDDEBUG
#define PKIX_BUILD_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BUILD_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BUILD_DEBUG(expr)
#define PKIX_BUILD_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTCHAINCHECKERDEBUG
#define PKIX_CERTCHAINCHECKER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTCHAINCHECKER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTCHAINCHECKER_DEBUG(expr)
#define PKIX_CERTCHAINCHECKER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_REVOCATIONCHECKERDEBUG
#define PKIX_REVOCATIONCHECKER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_REVOCATIONCHECKER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_REVOCATIONCHECKER_DEBUG(expr)
#define PKIX_REVOCATIONCHECKER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTSELECTORDEBUG
#define PKIX_CERTSELECTOR_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTSELECTOR_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTSELECTOR_DEBUG(expr)
#define PKIX_CERTSELECTOR_DEBUG_ARG(expr, arg)
#endif

#if PKIX_COMCERTSELPARAMSDEBUG
#define PKIX_COMCERTSELPARAMS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_COMCERTSELPARAMS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_COMCERTSELPARAMS_DEBUG(expr)
#define PKIX_COMCERTSELPARAMS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_TARGETCERTCHECKERSTATEDEBUG
#define PKIX_TARGETCERTCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_TARGETCERTCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_TARGETCERTCHECKERSTATE_DEBUG(expr)
#define PKIX_TARGETCERTCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BASICCONSTRAINTSCHECKERSTATEDEBUG
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG(expr)
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_INITIALIZEPARAMSDEBUG
#define PKIX_INITIALIZEPARAMS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_INITIALIZEPARAMS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_INITIALIZEPARAMS_DEBUG(expr)
#define PKIX_INITIALIZEPARAMS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTBASICCONSTRAINTSDEBUG
#define PKIX_CERTBASICCONSTRAINTS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTBASICCONSTRAINTS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTBASICCONSTRAINTS_DEBUG(expr)
#define PKIX_CERTBASICCONSTRAINTS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTNAMECONSTRAINTSDEBUG
#define PKIX_CERTNAMECONSTRAINTS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTNAMECONSTRAINTS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTNAMECONSTRAINTS_DEBUG(expr)
#define PKIX_CERTNAMECONSTRAINTS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTNAMECONSTRAINTSCHECKERSTATEDEBUG
#define PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_DEBUG(expr)
#define PKIX_CERTNAMECONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_SUBJALTNAMECHECKERSTATEDEBUG
#define PKIX_SUBJALTNAMECHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_SUBJALTNAMECHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_SUBJALTNAMECHECKERSTATE_DEBUG(expr)
#define PKIX_SUBJALTNAMECHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTPOLICYQUALIFIERDEBUG
#define PKIX_CERTPOLICYQUALIFIER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTPOLICYQUALIFIER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTPOLICYQUALIFIER_DEBUG(expr)
#define PKIX_CERTPOLICYQUALIFIER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTPOLICYINFODEBUG
#define PKIX_CERTPOLICYINFO_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTPOLICYINFO_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTPOLICYINFO_DEBUG(expr)
#define PKIX_CERTPOLICYINFO_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTPOLICYNODEDEBUG
#define PKIX_CERTPOLICYNODE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTPOLICYNODE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTPOLICYNODE_DEBUG(expr)
#define PKIX_CERTPOLICYNODE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTPOLICYCHECKERSTATEDEBUG
#define PKIX_CERTPOLICYCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTPOLICYCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTPOLICYCHECKERSTATE_DEBUG(expr)
#define PKIX_CERTPOLICYCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LIFECYCLEDEBUG
#define PKIX_LIFECYCLE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LIFECYCLE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LIFECYCLE_DEBUG(expr)
#define PKIX_LIFECYCLE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BASICCONSTRAINTSCHECKERSTATEDEBUG
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG(expr)
#define PKIX_BASICCONSTRAINTSCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CRLDEBUG
#define PKIX_CRL_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CRL_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CRL_DEBUG(expr)
#define PKIX_CRL_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CRLENTRYDEBUG
#define PKIX_CRLENTRY_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CRLENTRY_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CRLENTRY_DEBUG(expr)
#define PKIX_CRLENTRY_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CRLSELECTORDEBUG
#define PKIX_CRLSELECTOR_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CRLSELECTOR_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CRLSELECTOR_DEBUG(expr)
#define PKIX_CRLSELECTOR_DEBUG_ARG(expr, arg)
#endif

#if PKIX_COMCRLSELPARAMSDEBUG
#define PKIX_COMCRLSELPARAMS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_COMCRLSELPARAMS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_COMCRLSELPARAMS_DEBUG(expr)
#define PKIX_COMCRLSELPARAMS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTSTOREDEBUG
#define PKIX_CERTSTORE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTSTORE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTSTORE_DEBUG(expr)
#define PKIX_CERTSTORE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_COLLECTIONCERTSTORECONTEXTDEBUG
#define PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG(expr)
#define PKIX_COLLECTIONCERTSTORECONTEXT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_DEFAULTCRLCHECKERSTATEDEBUG
#define PKIX_DEFAULTCRLCHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_DEFAULTCRLCHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_DEFAULTCRLCHECKERSTATE_DEBUG(expr)
#define PKIX_DEFAULTCRLCHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CERTPOLICYMAPDEBUG
#define PKIX_CERTPOLICYMAP_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CERTPOLICYMAP_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CERTPOLICYMAP_DEBUG(expr)
#define PKIX_CERTPOLICYMAP_DEBUG_ARG(expr, arg)
#endif

#if PKIX_BUILDRESULTDEBUG
#define PKIX_BUILDRESULT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_BUILDRESULT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_BUILDRESULT_DEBUG(expr)
#define PKIX_BUILDRESULT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_FORWARDBUILDERSTATEDEBUG
#define PKIX_FORWARDBUILDERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_FORWARDBUILDERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_FORWARDBUILDERSTATE_DEBUG(expr)
#define PKIX_FORWARDBUILDERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_SIGNATURECHECKERSTATEDEBUG
#define PKIX_SIGNATURECHECKERSTATE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_SIGNATURECHECKERSTATE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_SIGNATURECHECKERSTATE_DEBUG(expr)
#define PKIX_SIGNATURECHECKERSTATE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_USERDEFINEDMODULESDEBUG
#define PKIX_USERDEFINEDMODULES_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_USERDEFINEDMODULES_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_USERDEFINEDMODULES_DEBUG(expr)
#define PKIX_USERDEFINEDMODULES_DEBUG_ARG(expr, arg)
#endif

#if PKIX_CONTEXTDEBUG
#define PKIX_CONTEXT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_CONTEXT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_CONTEXT_DEBUG(expr)
#define PKIX_CONTEXT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_DEFAULTREVOCATIONCHECKERDEBUG
#define PKIX_DEFAULTREVOCATIONCHECKER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_DEFAULTREVOCATIONCHECKER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_DEFAULTREVOCATIONCHECKER_DEBUG(expr)
#define PKIX_DEFAULTREVOCATIONCHECKER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LDAPREQUESTDEBUG
#define PKIX_LDAPREQUEST_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LDAPREQUEST_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LDAPREQUEST_DEBUG(expr)
#define PKIX_LDAPREQUEST_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LDAPRESPONSEDEBUG
#define PKIX_LDAPRESPONSE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LDAPRESPONSE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LDAPRESPONSE_DEBUG(expr)
#define PKIX_LDAPRESPONSE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LDAPCLIENTDEBUG
#define PKIX_LDAPCLIENT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LDAPCLIENT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LDAPCLIENT_DEBUG(expr)
#define PKIX_LDAPCLIENT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LDAPDEFAULTCLIENTDEBUG
#define PKIX_LDAPDEFAULTCLIENT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LDAPDEFAULTCLIENT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LDAPDEFAULTCLIENT_DEBUG(expr)
#define PKIX_LDAPDEFAULTCLIENT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_SOCKETDEBUG
#define PKIX_SOCKET_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_SOCKET_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_SOCKET_DEBUG(expr)
#define PKIX_SOCKET_DEBUG_ARG(expr, arg)
#endif

#if PKIX_RESOURCELIMITSDEBUG
#define PKIX_RESOURCELIMITS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_RESOURCELIMITS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_RESOURCELIMITS_DEBUG(expr)
#define PKIX_RESOURCELIMITS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_LOGGERDEBUG
#define PKIX_LOGGER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_LOGGER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_LOGGER_DEBUG(expr)
#define PKIX_LOGGER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_MONITORLOCKDEBUG
#define PKIX_MONITORLOCK_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_MONITORLOCK_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_MONITORLOCK_DEBUG(expr)
#define PKIX_MONITORLOCK_DEBUG_ARG(expr, arg)
#endif

#if PKIX_INFOACCESSDEBUG
#define PKIX_INFOACCESS_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_INFOACCESS_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_INFOACCESS_DEBUG(expr)
#define PKIX_INFOACCESS_DEBUG_ARG(expr, arg)
#endif

#if PKIX_AIAMGRDEBUG
#define PKIX_AIAMGR_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_AIAMGR_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_AIAMGR_DEBUG(expr)
#define PKIX_AIAMGR_DEBUG_ARG(expr, arg)
#endif

#if PKIX_OCSPCHECKERDEBUG
#define PKIX_OCSPCHECKER_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_OCSPCHECKER_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_OCSPCHECKER_DEBUG(expr)
#define PKIX_OCSPCHECKER_DEBUG_ARG(expr, arg)
#endif

#if PKIX_OCSPREQUESTDEBUG
#define PKIX_OCSPREQUEST_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_OCSPREQUEST_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_OCSPREQUEST_DEBUG(expr)
#define PKIX_OCSPREQUEST_DEBUG_ARG(expr, arg)
#endif

#if PKIX_OCSPRESPONSEDEBUG
#define PKIX_OCSPRESPONSE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_OCSPRESPONSE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_OCSPRESPONSE_DEBUG(expr)
#define PKIX_OCSPRESPONSE_DEBUG_ARG(expr, arg)
#endif

#if PKIX_HTTPDEFAULTCLIENTDEBUG
#define PKIX_HTTPDEFAULTCLIENT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_HTTPDEFAULTCLIENT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_HTTPDEFAULTCLIENT_DEBUG(expr)
#define PKIX_HTTPDEFAULTCLIENT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_HTTPCERTSTORECONTEXTDEBUG
#define PKIX_HTTPCERTSTORECONTEXT_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_HTTPCERTSTORECONTEXT_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_HTTPCERTSTORECONTEXT_DEBUG(expr)
#define PKIX_HTTPCERTSTORECONTEXT_DEBUG_ARG(expr, arg)
#endif

#if PKIX_VERIFYNODEDEBUG
#define PKIX_VERIFYNODE_DEBUG(expr) \
        PKIX_DEBUG(expr)
#define PKIX_VERIFYNODE_DEBUG_ARG(expr, arg) \
        PKIX_DEBUG_ARG(expr, arg)
#else
#define PKIX_VERIFYNODE_DEBUG(expr)
#define PKIX_VERIFYNODE_DEBUG_ARG(expr, arg)
#endif

/*
 * All object types register themselves with the system using a
 * pkix_ClassTable_Entry, which consists of a set of functions for that
 * type and an ASCII string (char *) which is used by the default
 * ToStringCallback (if necessary). System types register themselves directly
 * when their respective PKIX_"type"_RegisterSelf functions are called.
 * User-defined types can be registered using PKIX_PL_Object_RegisterType.
 * (see comments in pkix_pl_system.h)
 */

typedef struct pkix_ClassTable_EntryStruct pkix_ClassTable_Entry;
struct pkix_ClassTable_EntryStruct {
        char *description;
        PKIX_PL_DestructorCallback destructor;
        PKIX_PL_EqualsCallback equalsFunction;
        PKIX_PL_HashcodeCallback hashcodeFunction;
        PKIX_PL_ToStringCallback toStringFunction;
        PKIX_PL_ComparatorCallback comparator;
        PKIX_PL_DuplicateCallback duplicateFunction;
};

/*
 * PKIX_ERRORNAMES is an array of strings, with each string holding a
 * descriptive name for an error code. This is used by the default
 * PKIX_PL_Error_ToString function.
 */
#ifdef _WIN32
extern __declspec(dllimport) const char *PKIX_ERRORNAMES[PKIX_NUMERRORS];
#else
extern const char *PKIX_ERRORNAMES[PKIX_NUMERRORS];
#endif

#define PKIX_MAGIC_HEADER       (PKIX_UInt32) 0xBEEFC0DE

/* see source file for function documentation */

PKIX_Error *
pkix_IsCertSelfIssued(
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pSelfIssued,
        void *plContext);

PKIX_Error *
pkix_Throw(
        PKIX_UInt32 code,
        char *funcName,
        char *errorText,
        PKIX_Error *cause,
        PKIX_Error **pError,
        void *plContext);

PKIX_Error *
pkix_CheckTypes(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_UInt32 type,
        void *plContext);

PKIX_Error *
pkix_CheckType(
        PKIX_PL_Object *object,
        PKIX_UInt32 type,
        void *plContext);

PKIX_Error *
pkix_hash(
        const unsigned char *bytes,
        PKIX_UInt32 length,
        PKIX_UInt32 *hash,
        void *plContext);

PKIX_Error *
pkix_duplicateImmutable(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext);

PKIX_UInt32
pkix_countArray(void **array);

PKIX_UInt32
pkix_hex2i(char c);

char
pkix_i2hex(char c);

PKIX_Boolean
pkix_isPlaintext(unsigned char c, PKIX_Boolean debug);

PKIX_Error *
pkix_CacheCertChain_Lookup(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        PKIX_PL_Date *testDate,
        PKIX_Boolean *pFound,
        PKIX_BuildResult **pBuildResult,
        void *plContext);

PKIX_Error *
pkix_CacheCertChain_Remove(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        void *plContext);

PKIX_Error *
pkix_CacheCertChain_Add(
        PKIX_PL_Cert* targetCert,
        PKIX_List* anchors,
        PKIX_PL_Date *validityDate,
        PKIX_BuildResult *buildResult,
        void *plContext);

PKIX_Error *
pkix_CacheCert_Lookup(
        PKIX_CertStore *store,
        PKIX_ComCertSelParams *certSelParams,
        PKIX_PL_Date *testDate,
        PKIX_Boolean *pFound,
        PKIX_List** pCerts,
        void *plContext);

PKIX_Error *
pkix_CacheCert_Add(
        PKIX_CertStore *store,
        PKIX_ComCertSelParams *certSelParams,
        PKIX_List* certs,
        void *plContext);

PKIX_Error *
pkix_CacheCrlEntry_Lookup(
        PKIX_CertStore *store,
        PKIX_PL_X500Name *certIssuer,
        PKIX_PL_BigInt *certSerialNumber,
        PKIX_Boolean *pFound,
        PKIX_List** pCrlEntryList,
        void *plContext);

PKIX_Error *
pkix_CacheCrlEntry_Add(
        PKIX_CertStore *store,
        PKIX_PL_X500Name *certIssuer,
        PKIX_PL_BigInt *certSerialNumber,
        PKIX_List* crlEntryList,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_TOOLS_H */
