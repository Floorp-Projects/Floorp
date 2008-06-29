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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
/* $Id: lowkeyi.h,v 1.11 2007/06/13 00:24:56 rrelyea%redhat.com Exp $ */

#ifndef _LOWKEYI_H_
#define _LOWKEYI_H_

#include "prtypes.h"
#include "seccomon.h"
#include "secoidt.h"
#include "lowkeyti.h"

SEC_BEGIN_PROTOS

/*
 * See bugzilla bug 125359
 * Since NSS (via PKCS#11) wants to handle big integers as unsigned ints,
 * all of the templates above that en/decode into integers must be converted
 * from ASN.1's signed integer type.  This is done by marking either the
 * source or destination (encoding or decoding, respectively) type as
 * siUnsignedInteger.
 */
extern void prepare_low_rsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_pqg_params_for_asn1(PQGParams *params);
extern void prepare_low_dsa_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_dsa_priv_key_export_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_dh_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
#ifdef NSS_ENABLE_ECC
extern void prepare_low_ec_priv_key_for_asn1(NSSLOWKEYPrivateKey *key);
extern void prepare_low_ecparams_for_asn1(ECParams *params);
#endif /* NSS_ENABLE_ECC */

/*
** Destroy a private key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void nsslowkey_DestroyPrivateKey(NSSLOWKEYPrivateKey *key);

/*
** Destroy a public key object.
**	"key" the object
**	"freeit" if PR_TRUE then free the object as well as its sub-objects
*/
extern void nsslowkey_DestroyPublicKey(NSSLOWKEYPublicKey *key);

/*
** Return the modulus length of "pubKey".
*/
extern unsigned int nsslowkey_PublicModulusLen(NSSLOWKEYPublicKey *pubKey);


/*
** Return the modulus length of "privKey".
*/
extern unsigned int nsslowkey_PrivateModulusLen(NSSLOWKEYPrivateKey *privKey);


/*
** Convert a low private key "privateKey" into a public low key
*/
extern NSSLOWKEYPublicKey 
		*nsslowkey_ConvertToPublicKey(NSSLOWKEYPrivateKey *privateKey);

/* Make a copy of a low private key in it's own arena.
 * a return of NULL indicates an error.
 */
extern NSSLOWKEYPrivateKey *
nsslowkey_CopyPrivateKey(NSSLOWKEYPrivateKey *privKey);


SEC_END_PROTOS

#endif /* _LOWKEYI_H_ */
