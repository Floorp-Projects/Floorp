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
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

/* This key is the 1024-bit test key used for speed testing of RSA private 
** key ops.
*/
#include "seccomon.h"
#include "secoidt.h"
#include "keyt.h"

#define CONST

static CONST unsigned char default_n[128] = {
/* Modulus goes here */ 0
};

static CONST unsigned char default_e[3] = { 0x01,0x00,0x01 };

static CONST unsigned char default_d[128] = {
/* private exponent goes here */ 0
};

static CONST unsigned char default_p[64] = {
/* p goes here */ 0
};

static CONST unsigned char default_q[64] = {
/* q goes here */ 0
};

static CONST unsigned char default_dModP[64] = {
/* d mod (p-1) goes here */ 0
};

static CONST unsigned char default_dModQ[64] = {
/* d mod (q-1) goes here */ 0
};

static CONST unsigned char default_qInvModP[64] = {
/* q**-1 mod p goes here. */ 0
};

static struct SECKEYLowPrivateKeyStr rsaPriv;

SECKEYLowPrivateKey *
getDefaultRSAPrivateKey(void)
{
    if (rsaPriv.keyType != rsaKey) {

	/* leaving arena uninitialized. It isn't used in this test. */

	rsaPriv.keyType                     = rsaKey;

	/* leaving arena   uninitialized. It isn't used. */
	/* leaving version uninitialized. It isn't used. */

	rsaPriv.u.rsa.modulus.data          =        default_n;
	rsaPriv.u.rsa.modulus.len           = sizeof default_n;
	rsaPriv.u.rsa.publicExponent.data   =        default_e;
	rsaPriv.u.rsa.publicExponent.len    = sizeof default_e;
	rsaPriv.u.rsa.privateExponent.data  =        default_d;
	rsaPriv.u.rsa.privateExponent.len   = sizeof default_d;
	rsaPriv.u.rsa.prime1.data           =        default_p;
	rsaPriv.u.rsa.prime1.len            = sizeof default_p;
	rsaPriv.u.rsa.prime2.data           =        default_q;
	rsaPriv.u.rsa.prime2.len            = sizeof default_q;
	rsaPriv.u.rsa.exponent1.data        =        default_dModP;
	rsaPriv.u.rsa.exponent1.len         = sizeof default_dModP;
	rsaPriv.u.rsa.exponent2.data        =        default_dModQ;
	rsaPriv.u.rsa.exponent2.len         = sizeof default_dModQ;
	rsaPriv.u.rsa.coefficient.data      =        default_qInvModP;
	rsaPriv.u.rsa.coefficient.len       = sizeof default_qInvModP;
    }
    return &rsaPriv;
}

static struct SECKEYLowPublicKeyStr rsaPub;

SECKEYLowPublicKey *
getDefaultRSAPublicKey(void)
{
    if (rsaPub.keyType != rsaKey) {

	rsaPub.keyType			   = rsaKey;

	rsaPub.u.rsa.modulus.data          =        default_n;
	rsaPub.u.rsa.modulus.len           = sizeof default_n;

	rsaPub.u.rsa.publicExponent.data   =        default_e;
	rsaPub.u.rsa.publicExponent.len    = sizeof default_e;
    }
    return &rsaPub;
}

