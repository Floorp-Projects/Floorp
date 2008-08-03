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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: errorval.c,v $ $Revision: 1.12 $ $Date: 2007/08/09 22:36:15 $";
#endif /* DEBUG */

/*
 * errorval.c
 *
 * This file contains the actual error constants used in NSS.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

const NSSError NSS_ERROR_NO_ERROR                       =  0;
const NSSError NSS_ERROR_INTERNAL_ERROR                 =  1;
const NSSError NSS_ERROR_NO_MEMORY                      =  2;
const NSSError NSS_ERROR_INVALID_POINTER                =  3;
const NSSError NSS_ERROR_INVALID_ARENA                  =  4;
const NSSError NSS_ERROR_INVALID_ARENA_MARK             =  5;
const NSSError NSS_ERROR_DUPLICATE_POINTER              =  6;
const NSSError NSS_ERROR_POINTER_NOT_REGISTERED         =  7;
const NSSError NSS_ERROR_TRACKER_NOT_EMPTY              =  8;
const NSSError NSS_ERROR_TRACKER_NOT_INITIALIZED        =  9;
const NSSError NSS_ERROR_ARENA_MARKED_BY_ANOTHER_THREAD = 10;
const NSSError NSS_ERROR_VALUE_TOO_LARGE                = 11;
const NSSError NSS_ERROR_UNSUPPORTED_TYPE               = 12;
const NSSError NSS_ERROR_BUFFER_TOO_SHORT               = 13;
const NSSError NSS_ERROR_INVALID_ATOB_CONTEXT           = 14;
const NSSError NSS_ERROR_INVALID_BASE64                 = 15;
const NSSError NSS_ERROR_INVALID_BTOA_CONTEXT           = 16;
const NSSError NSS_ERROR_INVALID_ITEM                   = 17;
const NSSError NSS_ERROR_INVALID_STRING                 = 18;
const NSSError NSS_ERROR_INVALID_ASN1ENCODER            = 19;
const NSSError NSS_ERROR_INVALID_ASN1DECODER            = 20;

const NSSError NSS_ERROR_INVALID_BER                    = 21;
const NSSError NSS_ERROR_INVALID_ATAV                   = 22;
const NSSError NSS_ERROR_INVALID_ARGUMENT               = 23;
const NSSError NSS_ERROR_INVALID_UTF8                   = 24;
const NSSError NSS_ERROR_INVALID_NSSOID                 = 25;
const NSSError NSS_ERROR_UNKNOWN_ATTRIBUTE              = 26;

const NSSError NSS_ERROR_NOT_FOUND                      = 27;

const NSSError NSS_ERROR_INVALID_PASSWORD               = 28;
const NSSError NSS_ERROR_USER_CANCELED                  = 29;

const NSSError NSS_ERROR_MAXIMUM_FOUND                  = 30;

const NSSError NSS_ERROR_CERTIFICATE_ISSUER_NOT_FOUND   = 31;

const NSSError NSS_ERROR_CERTIFICATE_IN_CACHE           = 32;

const NSSError NSS_ERROR_HASH_COLLISION                 = 33;
const NSSError NSS_ERROR_DEVICE_ERROR                   = 34;
const NSSError NSS_ERROR_INVALID_CERTIFICATE            = 35;
const NSSError NSS_ERROR_BUSY                           = 36;
const NSSError NSS_ERROR_ALREADY_INITIALIZED            = 37;

const NSSError NSS_ERROR_PKCS11                         = 38;

