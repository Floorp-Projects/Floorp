/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * ERROR codes in pk12util
 * - should be organized better later
 */
#define PK12UERR_USER_CANCELLED 1
#define PK12UERR_USAGE 2
#define PK12UERR_CERTDB_OPEN 8
#define PK12UERR_KEYDB_OPEN 9
#define PK12UERR_INIT_FILE 10
#define PK12UERR_UNICODECONV 11
#define PK12UERR_TMPDIGCREATE 12
#define PK12UERR_PK11GETSLOT 13
#define PK12UERR_PK12DECODESTART 14
#define PK12UERR_IMPORTFILEREAD 15
#define PK12UERR_DECODE 16
#define PK12UERR_DECODEVERIFY 17
#define PK12UERR_DECODEVALIBAGS 18
#define PK12UERR_DECODEIMPTBAGS 19
#define PK12UERR_CERTALREADYEXISTS 20
#define PK12UERR_PATCHDB 22
#define PK12UERR_GETDEFCERTDB 23
#define PK12UERR_FINDCERTBYNN 24
#define PK12UERR_EXPORTCXCREATE 25
#define PK12UERR_PK12ADDPWDINTEG 26
#define PK12UERR_CERTKEYSAFE 27
#define PK12UERR_ADDCERTKEY 28
#define PK12UERR_ENCODE 29
#define PK12UERR_INVALIDALGORITHM 30


/* additions for importing and exporting PKCS 12 files */
typedef struct p12uContextStr {
    char        *filename;    /* name of file */
	PRFileDesc  *file;        /* pointer to file */
	PRBool       error;       /* error occurred? */
	int          errorValue;  /* which error occurred? */
} p12uContext;
