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
 * this file maps PKCS11 Errors into SECErrors
 *  This is an information reducing process, since most errors are reflected
 *  back to the user (the user doesn't care about invalid flags, or active
 *  operations). If any of these errors need more detail in the upper layers
 *  which call PK11 library functions, we can add more SEC_ERROR_XXX functions
 *  and change there mappings here.
 */
#include "pkcs11t.h"
#include "pk11func.h"
#include "secerr.h"

#ifdef PK11_ERROR_USE_ARRAY 

/*
 * build a static array of entries...
 */
static struct {
	CK_RV pk11_error;
	int   sec_error;
} pk11_error_map = {
#define MAPERROR(x,y) {x, y},

#else

/* the default is to use a big switch statement */
int
PK11_MapError(CK_RV rv) {

	switch (rv) {
#define MAPERROR(x,y) case x: return y;

#endif

/* the guts mapping */
	MAPERROR(CKR_OK, 0)
	MAPERROR(CKR_CANCEL, SEC_ERROR_IO)
	MAPERROR(CKR_HOST_MEMORY, SEC_ERROR_NO_MEMORY)
	MAPERROR(CKR_SLOT_ID_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_ATTRIBUTE_READ_ONLY, SEC_ERROR_READ_ONLY)
	MAPERROR(CKR_ATTRIBUTE_SENSITIVE, SEC_ERROR_IO) /* XX SENSITIVE */
	MAPERROR(CKR_ATTRIBUTE_TYPE_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_ATTRIBUTE_VALUE_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_DATA_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_DATA_LEN_RANGE, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_DEVICE_ERROR, SEC_ERROR_IO)
	MAPERROR(CKR_DEVICE_MEMORY, SEC_ERROR_NO_MEMORY)
	MAPERROR(CKR_DEVICE_REMOVED, SEC_ERROR_NO_TOKEN)
	MAPERROR(CKR_ENCRYPTED_DATA_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_ENCRYPTED_DATA_LEN_RANGE, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_FUNCTION_CANCELED, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_FUNCTION_NOT_PARALLEL, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_KEY_HANDLE_INVALID, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_KEY_SIZE_RANGE, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_KEY_TYPE_INCONSISTENT, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_MECHANISM_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_MECHANISM_PARAM_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_NO_EVENT, SEC_ERROR_NO_EVENT)
	MAPERROR(CKR_OBJECT_HANDLE_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_OPERATION_ACTIVE, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_OPERATION_NOT_INITIALIZED,SEC_ERROR_LIBRARY_FAILURE )
	MAPERROR(CKR_PIN_INCORRECT, SEC_ERROR_BAD_PASSWORD)
	MAPERROR(CKR_PIN_INVALID, SEC_ERROR_BAD_PASSWORD)
	MAPERROR(CKR_PIN_LEN_RANGE, SEC_ERROR_BAD_PASSWORD)
	MAPERROR(CKR_SESSION_CLOSED, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_SESSION_COUNT, SEC_ERROR_NO_MEMORY) /* XXXX? */
	MAPERROR(CKR_SESSION_HANDLE_INVALID, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_SESSION_PARALLEL_NOT_SUPPORTED, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_SESSION_READ_ONLY, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_SIGNATURE_INVALID, SEC_ERROR_BAD_SIGNATURE)
	MAPERROR(CKR_SIGNATURE_LEN_RANGE, SEC_ERROR_BAD_SIGNATURE)
	MAPERROR(CKR_TEMPLATE_INCOMPLETE, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_TEMPLATE_INCONSISTENT, SEC_ERROR_BAD_DATA)
	MAPERROR(CKR_TOKEN_NOT_PRESENT, SEC_ERROR_NO_TOKEN)
	MAPERROR(CKR_TOKEN_NOT_RECOGNIZED, SEC_ERROR_IO)
	MAPERROR(CKR_TOKEN_WRITE_PROTECTED, SEC_ERROR_READ_ONLY)
	MAPERROR(CKR_UNWRAPPING_KEY_HANDLE_INVALID, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_UNWRAPPING_KEY_SIZE_RANGE, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_USER_ALREADY_LOGGED_IN, 0)
	MAPERROR(CKR_USER_NOT_LOGGED_IN, SEC_ERROR_LIBRARY_FAILURE) /* XXXX */
	MAPERROR(CKR_USER_PIN_NOT_INITIALIZED, SEC_ERROR_NO_TOKEN)
	MAPERROR(CKR_USER_TYPE_INVALID, SEC_ERROR_LIBRARY_FAILURE)
	MAPERROR(CKR_WRAPPED_KEY_INVALID, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_WRAPPED_KEY_LEN_RANGE, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_WRAPPING_KEY_HANDLE_INVALID, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_WRAPPING_KEY_SIZE_RANGE, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_WRAPPING_KEY_TYPE_INCONSISTENT, SEC_ERROR_INVALID_KEY)
	MAPERROR(CKR_VENDOR_DEFINED, SEC_ERROR_LIBRARY_FAILURE)


#ifdef PK11_ERROR_USE_ARRAY 
};

int
PK11_MapError(CK_RV rv) {
    int size = sizeof(pk11_error_map)/sizeof(pk11_error_map[0]);

    for (i=0; i < size; i++) {
	if (pk11_error_map[i].pk11_error == rv) {
	    return pk11_error_map[i].sec_error;
	}
    }
    return SEC_ERROR_IO;
 }


#else

    default:
	break;
    }
    return SEC_ERROR_IO;
}


#endif
