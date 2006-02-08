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
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bob Relyea, Red Hat
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
/* $Id: keyi.h,v 1.1 2006/02/08 06:14:07 rrelyea%redhat.com Exp $ */

#ifndef _KEYI_H_
#define _KEYI_H_


SEC_BEGIN_PROTOS
/* NSS private functions */
/* map an oid to a keytype... actually this function and it's converse
 *  are good candidates for public functions..  */
KeyType seckey_GetKeyType(SECOidTag pubKeyOid);

/* extract the 'encryption' (could be signing) and hash oids from and
 * algorithm, key and parameters (parameters is the parameters field
 * of a algorithm ID structure (SECAlgorithmID)*/
SECStatus sec_DecodeSigAlg(const SECKEYPublicKey *key, SECOidTag sigAlg,
             const SECItem *param, SECOidTag *encalg, SECOidTag *hashalg);

SEC_END_PROTOS

#endif /* _KEYHI_H_ */
