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


#ifndef _PKCS12_H_
#define _PKCS12_H_

#include "pkcs12t.h"
#include "p12.h"

SEC_BEGIN_PROTOS

typedef SECItem * (* SEC_PKCS12GetPassword)(void *arg);

/* Decode functions */
/* Import a PFX item.  
 *	der_pfx is the der-encoded pfx item to import.
 *	pbef, and pbefarg are used to retrieve passwords for the HMAC,
 *	    and any passwords needed for passing to PKCS5 encryption 
 *	    routines.
 *	algorithm is the algorithm by which private keys are stored in
 *	    the key database.  this could be a specific algorithm or could
 *	    be based on a global setting.
 *	slot is the slot to where the certificates will be placed.  if NULL,
 *	    the internal key slot is used.
 * If the process is successful, a SECSuccess is returned, otherwise
 * a failure occurred.
 */ 
SECStatus
SEC_PKCS12PutPFX(SECItem *der_pfx, SECItem *pwitem,
		 SEC_PKCS12NicknameCollisionCallback ncCall,
		 PK11SlotInfo *slot, void *wincx);

/* check the first two bytes of a file to make sure that it matches
 * the desired header for a PKCS 12 file
 */
PRBool SEC_PKCS12ValidData(char *buf, int bufLen, long int totalLength);

SEC_END_PROTOS

#endif
