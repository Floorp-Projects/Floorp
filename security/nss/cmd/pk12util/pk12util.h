/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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


/* additions for importing and exporting PKCS 12 files */
typedef struct p12uContextStr {
    char        *filename;    /* name of file */
	PRFileDesc  *file;        /* pointer to file */
	PRBool       error;       /* error occurred? */
	int          errorValue;  /* which error occurred? */
} p12uContext;
