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

#ifndef _RIJNDAEL_H_
#define _RIJNDAEL_H_ 1

#define RIJNDAEL_MIN_BLOCKSIZE 16 /* bytes */
#define RIJNDAEL_MAX_BLOCKSIZE 32 /* bytes */

typedef SECStatus AESFunc(AESContext *cx, unsigned char *output,
                          unsigned int *outputLen, unsigned int maxOutputLen,
                          const unsigned char *input, unsigned int inputLen, 
                          unsigned int blocksize);

typedef SECStatus AESBlockFunc(AESContext *cx, 
                               unsigned char *output,
                               const unsigned char *input);

/* AESContextStr
 *
 * Values which maintain the state for Rijndael encryption/decryption.
 *
 * iv          - initialization vector for CBC mode
 * Nb          - the number of bytes in a block, specified by user
 * Nr          - the number of rounds, specified by a table
 * expandedKey - the round keys in 4-byte words, the length is Nr * Nb
 * worker      - the encryption/decryption function to use with this context
 */
struct AESContextStr
{
    unsigned int   Nb;
    unsigned int   Nr;
    PRUint32      *expandedKey;
    AESFunc       *worker;
    unsigned char iv[RIJNDAEL_MAX_BLOCKSIZE];
};

/* RIJNDAEL_NUM_ROUNDS
 *
 * Number of rounds per execution
 * Nk - number of key bytes
 * Nb - blocksize (in bytes)
 */
#define RIJNDAEL_NUM_ROUNDS(Nk, Nb) \
    (PR_MAX(Nk, Nb) + 6)

/* RIJNDAEL_NUM_ROUNDS
 *
 * Maximum number of bytes in the state (spec includes up to 256-bit block
 * size)
 */
#define RIJNDAEL_MAX_STATE_SIZE 32

#endif /* _RIJNDAEL_H_ */
