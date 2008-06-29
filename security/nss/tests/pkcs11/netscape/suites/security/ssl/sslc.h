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
#ifndef SSLC_H
#define SSLC_H

#include "ssls.h"

struct cipherspec {
  int sslversion;  /* either 2 or 3 */
  int exportable;  /* 0=domestic cipher,  1=exportable */
  int ks,sks;      /* key size, secret key size (bits) */
  char *name;      /* name expected from SecurityStatus */
  int enableid;    /* the cipher id used by SSL_EnableCipher */
  int on;          /* 0= do not enable this cipher, 1 = enable */
};


/* Ugly way to generate code to fill in cipher_array struct */
/* I wanted to make this part of the static structure initialization,
   but some compilers complain that the .on field is not constant */

#define CIPHER(p_sslversion,p_policy,p_ks,p_sks,p_name,p_x) {\
 cipher_array[i].sslversion = p_sslversion; \
 cipher_array[i].exportable = p_policy;     \
 cipher_array[i].ks         = p_ks;         \
 cipher_array[i].sks        = p_sks;        \
 cipher_array[i].name       = p_name;       \
 cipher_array[i].enableid   = SSL_ ## p_x;  \
 cipher_array[i].on         = REP_Cipher_ ## p_x; \
 i++; }

/* A DIPHER is a disabled-cipher (don't run the test suite) */
#define DIPHER(sslversion,policy,ks,sks,name,x)  ;


/* These constants are indexes into the 'nicknames' array */

#define NO_CERT                       -1
#define CLIENT_CERT_VERISIGN          1
#define CLIENT_CERT_HARDCOREII_1024   2
#define CLIENT_CERT_HARDCOREII_512    3
#define CLIENT_CERT_SPARK             4
#define SERVER_CERT_HARDCOREII_512    5
#define SERVER_CERT_VERISIGN_REGULAR  6
#define SERVER_CERT_VERISIGN_STEPUP   7
#define SERVER_CERT_SPARK             8
#define MAX_NICKNAME                  10

extern struct cipherspec cipher_array[];
extern int cipher_array_size;

extern void ClearCiphers();
extern void EnableCiphers();
extern void SetPolicy();
extern int  Version2Enable();
extern int  Version3Enable();
extern int  Version23Clear();
extern char *nicknames[];
extern void SetupNickNames();
extern int  SetServerSecParms(struct ThreadData *td);


#endif
/* SSLC_H */


