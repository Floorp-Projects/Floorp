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

#ifndef NSSASN1T_H
#define NSSASN1T_H

#ifdef DEBUG
static const char NSSASN1T_CVS_ID[] = "@(#) $RCSfile: nssasn1t.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:57:16 $ $Name:  $";
#endif /* DEBUG */

/*
 * nssasn1t.h
 *
 * This file contains the public types related to our ASN.1 encoder 
 * and decoder.
 */

PR_BEGIN_EXTERN_C

/*
 * NSSASN1EncodingType
 *
 * This type enumerates specific types of ASN.1 encodings.
 */

typedef enum {
  NSSASN1BER,               /* Basic Encoding Rules */
  NSSASN1CER,               /* Canonical Encoding Rules */
  NSSASN1DER,               /* Distinguished Encoding Rules */
  NSSASN1LWER,              /* LightWeight Encoding Rules */
  NSSASN1PER,               /* Packed Encoding Rules */
  NSSASN1UnknownEncoding = -1
} NSSASN1EncodingType;

PR_END_EXTERN_C

#endif /* NSSASN1T_H */
