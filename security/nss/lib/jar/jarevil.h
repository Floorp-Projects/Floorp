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
 *  jarevil.h
 *
 *  dot H file for calls to mozilla thread
 *  from within jarver.c 
 *
 */

#include "certt.h"
#include "secpkcs7.h"

extern SECStatus jar_moz_encode
      (
      SEC_PKCS7ContentInfo *cinfo,
      SEC_PKCS7EncoderOutputCallback  outputfn,
      void *outputarg,
      PK11SymKey *bulkkey,
      SECKEYGetPasswordKey pwfn,
      void *pwfnarg
      );

extern SECStatus jar_moz_verify
      (
      SEC_PKCS7ContentInfo *cinfo,
      SECCertUsage certusage,
      SECItem *detached_digest,
      HASH_HashType digest_type,
      PRBool keepcerts
      );

extern CERTCertificate *jar_moz_nickname 
   (CERTCertDBHandle *certdb, char *nickname);

extern SECStatus jar_moz_perm 
  (CERTCertificate *cert, char *nickname, CERTCertTrust *trust);
 
extern CERTCertificate *jar_moz_certkey 
  (CERTCertDBHandle *certdb, CERTIssuerAndSN *seckey);

extern CERTCertificate *jar_moz_issuer (CERTCertificate *cert);

extern CERTCertificate *jar_moz_dup (CERTCertificate *cert);

