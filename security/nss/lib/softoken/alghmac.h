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

#ifndef _ALGHMAC_H_
#define _ALGHMAC_H_

typedef struct HMACContextStr HMACContext;

SEC_BEGIN_PROTOS

/* destroy HMAC context */
extern void
HMAC_Destroy(HMACContext *cx);

/* create HMAC context
 *  hash_alg	the algorithm with which the HMAC is performed.  This 
 *		should be, SEC_OID_MD5, SEC_OID_SHA1, or SEC_OID_MD2.
 *  secret	the secret with which the HMAC is performed.
 *  secret_len	the length of the secret, limited to at most 64 bytes.
 *
 * NULL is returned if an error occurs or the secret is > 64 bytes.
 */
extern HMACContext *
HMAC_Create(const SECHashObject *hashObj, const unsigned char *secret, 
	    unsigned int secret_len);

/* reset HMAC for a fresh round */
extern void
HMAC_Begin(HMACContext *cx);

/* update HMAC 
 *  cx		HMAC Context
 *  data	the data to perform HMAC on
 *  data_len	the length of the data to process
 */
extern void 
HMAC_Update(HMACContext *cx, const unsigned char *data, unsigned int data_len);

/* Finish HMAC -- place the results within result
 *  cx		HMAC context
 *  result	buffer for resulting hmac'd data
 *  result_len	where the resultant hmac length is stored
 *  max_result_len  maximum possible length that can be stored in result
 */
extern SECStatus
HMAC_Finish(HMACContext *cx, unsigned char *result, unsigned int *result_len,
	    unsigned int max_result_len);

/* clone a copy of the HMAC state.  this is usefult when you would
 * need to keep a running hmac but also need to extract portions 
 * partway through the process.
 */
extern HMACContext *
HMAC_Clone(HMACContext *cx);

SEC_END_PROTOS

#endif
