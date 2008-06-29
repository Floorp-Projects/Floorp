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
 * The Original Code is the Camellia code.
 *
 * The Initial Developer of the Original Code is
 * NTT(Nippon Telegraph and Telephone Corporation).
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
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
 * $Id: camellia.h,v 1.1 2007/02/28 19:47:37 rrelyea%redhat.com Exp $
 */

#ifndef _CAMELLIA_H_
#define _CAMELLIA_H_ 1

#define CAMELLIA_BLOCK_SIZE 16  /* bytes */
#define CAMELLIA_MIN_KEYSIZE 16 /* bytes */
#define CAMELLIA_MAX_KEYSIZE 32 /* bytes */

#define CAMELLIA_MAX_EXPANDEDKEY (34*2) /* 32bit unit */

typedef PRUint32 KEY_TABLE_TYPE[CAMELLIA_MAX_EXPANDEDKEY];

typedef SECStatus CamelliaFunc(CamelliaContext *cx, unsigned char *output,
			       unsigned int *outputLen,
			       unsigned int maxOutputLen,
			       const unsigned char *input,
			       unsigned int inputLen);

typedef SECStatus CamelliaBlockFunc(const PRUint32 *subkey, 
				    unsigned char *output,
				    const unsigned char *input);

/* CamelliaContextStr
 *
 * Values which maintain the state for Camellia encryption/decryption.
 *
 * keysize     - the number of key bits
 * worker      - the encryption/decryption function to use with this context
 * iv          - initialization vector for CBC mode
 * expandedKey - the round keys in 4-byte words
 */
struct CamelliaContextStr
{
    PRUint32  keysize; /* bytes */
    CamelliaFunc  *worker;
    PRUint32      expandedKey[CAMELLIA_MAX_EXPANDEDKEY];
    PRUint8 iv[CAMELLIA_BLOCK_SIZE];
};

#endif /* _CAMELLIA_H_ */
