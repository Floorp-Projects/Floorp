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

#ifndef ASN1M_H
#define ASN1M_H

#ifdef DEBUG
static const char ASN1M_CVS_ID[] = "@(#) $RCSfile: asn1m.h,v $ $Revision: 1.3 $ $Date: 2005/01/20 02:25:44 $";
#endif /* DEBUG */

/*
 * asn1m.h
 *
 * This file contains the ASN.1 encoder/decoder routines available
 * only within the ASN.1 module itself.
 */

#ifndef ASN1_H
#include "asn1.h"
#endif /* ASN1_H */

PR_BEGIN_EXTERN_C

/*
 * nssasn1_number_length
 *
 */

NSS_EXTERN PRUint32
nssasn1_length_length
(
  PRUint32 number
);

/*
 * nssasn1_get_subtemplate
 *
 */

NSS_EXTERN const nssASN1Template *
nssasn1_get_subtemplate
(
  const nssASN1Template template[],
  void *thing,
  PRBool encoding
);

PR_END_EXTERN_C

#endif /* ASN1M_H */
