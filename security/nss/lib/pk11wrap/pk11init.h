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
 * Internal header file included in pk11wrap dir, or in softoken
 */
#ifndef _PK11_INIT_H_
#define _PK11_INIT_H_ 1

/* hold slot default flags until we initialize a slot. This structure is only
 * useful between the time we define a module (either by hand or from the
 * database) and the time the module is loaded. Not reference counted  */
struct PK11PreSlotInfoStr {
    CK_SLOT_ID slotID;  	/* slot these flags are for */
    unsigned long defaultFlags; /* bit mask of default implementation this slot
				 * provides */
    int askpw;			/* slot specific password bits */
    long timeout;		/* slot specific timeout value */
    char hasRootCerts;		/* is this the root cert PKCS #11 module? */
    char hasRootTrust;		/* is this the root cert PKCS #11 module? */
};

#define SECMOD_SLOT_FLAGS "slotFlags=[RSA,DSA,DH,RC2,RC4,DES,RANDOM,SHA1,MD5,MD2,SSL,TLS,AES]"

#define SECMOD_MAKE_NSS_FLAGS(fips,slot) \
"Flags=internal,critical"fips" slotparams=("#slot"={"SECMOD_SLOT_FLAGS"})"

#define SECMOD_INT_NAME "NSS Internal PKCS #11 Module"
#define SECMOD_INT_FLAGS SECMOD_MAKE_NSS_FLAGS("",1)
#define SECMOD_FIPS_NAME "NSS Internal FIPS PKCS #11 Module"
#define SECMOD_FIPS_FLAGS SECMOD_MAKE_NSS_FLAGS(",fips",3)


#endif /* _PK11_INIT_H_ 1 */
