/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * errorval.c
 *
 * This file contains the actual error constants used in NSS.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

/* clang-format off */

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

/* clang-format on */