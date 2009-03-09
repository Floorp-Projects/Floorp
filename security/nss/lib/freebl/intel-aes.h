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
 * The Initial Developer of the Original Code is Red Hat, Inc, 2008.
 *
 * Contributor(s):
 *	Ulrich Drepper <drepper@redhat.com>
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

/* Prototypes of the functions defined in the assembler file.  */
void intel_aes_encrypt_init_128(const unsigned char *key, PRUint32 *expanded);
void intel_aes_encrypt_init_192(const unsigned char *key, PRUint32 *expanded);
void intel_aes_encrypt_init_256(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_128(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_192(const unsigned char *key, PRUint32 *expanded);
void intel_aes_decrypt_init_256(const unsigned char *key, PRUint32 *expanded);
SECStatus intel_aes_encrypt_ecb_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_128(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_ecb_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_192(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_ecb_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_ecb_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_encrypt_cbc_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);
SECStatus intel_aes_decrypt_cbc_256(AESContext *cx, unsigned char *output,
				    unsigned int *outputLen,
				    unsigned int maxOutputLen,
				    const unsigned char *input,
				    unsigned int inputLen,
				    unsigned int blocksize);


#define intel_aes_ecb_worker(encrypt, keysize) \
  ((encrypt)						\
   ? ((keysize) == 16 ? intel_aes_encrypt_ecb_128 :	\
      (keysize) == 24 ? intel_aes_encrypt_ecb_192 :	\
      intel_aes_encrypt_ecb_256)			\
   : ((keysize) == 16 ? intel_aes_decrypt_ecb_128 :	\
      (keysize) == 24 ? intel_aes_decrypt_ecb_192 :	\
      intel_aes_decrypt_ecb_256))


#define intel_aes_cbc_worker(encrypt, keysize) \
  ((encrypt)						\
   ? ((keysize) == 16 ? intel_aes_encrypt_cbc_128 :	\
      (keysize) == 24 ? intel_aes_encrypt_cbc_192 :	\
      intel_aes_encrypt_cbc_256)			\
   : ((keysize) == 16 ? intel_aes_decrypt_cbc_128 :	\
      (keysize) == 24 ? intel_aes_decrypt_cbc_192 :	\
      intel_aes_decrypt_cbc_256))


#define intel_aes_init(encrypt, keysize) \
  do {					 			\
      if (encrypt) {			 			\
	  if (keysize == 16)					\
	      intel_aes_encrypt_init_128(key, cx->expandedKey);	\
	  else if (keysize == 24)				\
	      intel_aes_encrypt_init_192(key, cx->expandedKey);	\
	  else							\
	      intel_aes_encrypt_init_256(key, cx->expandedKey);	\
      } else {							\
	  if (keysize == 16)					\
	      intel_aes_decrypt_init_128(key, cx->expandedKey);	\
	  else if (keysize == 24)				\
	      intel_aes_decrypt_init_192(key, cx->expandedKey);	\
	  else							\
	      intel_aes_decrypt_init_256(key, cx->expandedKey);	\
      }								\
  } while (0)
