/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */

/*
 * Functions used by https servers to send (download) pre-encrypted files
 * over SSL connections that use Fortezza ciphersuites.
 *
 * ***** BEGIN LICENSE BLOCK *****
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

#include "cert.h"
#include "ssl.h"
#include "keyhi.h"
#include "secitem.h"
#include "sslimpl.h"
#include "pkcs11t.h"
#include "preenc.h"
#include "pk11func.h"

static unsigned char fromHex(char x) {
    if ((x >= '0') && (x <= '9')) return x-'0';
    if ((x >= 'a') && (x <= 'f')) return x-'a'+10;
    return x-'A'+10;
}

PEHeader *SSL_PreencryptedStreamToFile(PRFileDesc *fd, PEHeader *inHeader, 
								int *headerSize)
{
    PK11SymKey *key, *tek, *Ks;
    sslSocket *ss;
    PK11SlotInfo *slot;
    CK_TOKEN_INFO info;
    int oldHeaderSize;
    PEHeader *header;
    SECStatus rv;
    SECItem item;
    int i;
    
    if (fd == NULL) {
        /* XXX set an error */
        return NULL;
    }
    
    ss = ssl_FindSocket(fd);
    if (ss == NULL) {
        return NULL;
    }
    
    PORT_Assert(ss->ssl3 != NULL);
    if (ss->ssl3 == NULL) {
	return NULL;
    }

    if (GetInt2(inHeader->magic) != PRE_MAGIC) {
	return NULL;
    }

    oldHeaderSize = GetInt2(inHeader->len);
    header = (PEHeader *) PORT_ZAlloc(oldHeaderSize);
    if (header == NULL) {
	return NULL;
    }

    switch (GetInt2(inHeader->type)) {
    case PRE_FORTEZZA_FILE:
    case PRE_FORTEZZA_GEN_STREAM:
    case PRE_FIXED_FILE:
    case PRE_RSA_FILE:
    default:
	*headerSize = oldHeaderSize;
	PORT_Memcpy(header,inHeader,oldHeaderSize);
	return header;

    case PRE_FORTEZZA_STREAM:
	*headerSize = PE_BASE_HEADER_LEN + sizeof(PEFortezzaHeader);
        PutInt2(header->magic,PRE_MAGIC);
	PutInt2(header->len,*headerSize);
	PutInt2(header->type, PRE_FORTEZZA_FILE);
	PORT_Memcpy(header->version,inHeader->version,sizeof(header->version));
	PORT_Memcpy(header->u.fortezza.hash,inHeader->u.fortezza.hash,
					     sizeof(header->u.fortezza.hash));
	PORT_Memcpy(header->u.fortezza.iv,inHeader->u.fortezza.iv,
					      sizeof(header->u.fortezza.iv));

	/* get the kea context from the session */
	tek = ss->ssl3->fortezza.tek;
	if (tek == NULL) {
	    PORT_Free(header);
	    return NULL;
	}


	/* get the slot and the serial number */
	slot = PK11_GetSlotFromKey(tek);
	if (slot == NULL) {
	    PORT_Free(header);
	    return NULL;
	}
	rv = PK11_GetTokenInfo(slot,&info);
	if (rv != SECSuccess) {
	    PORT_Free(header);
	    PK11_FreeSlot(slot);
	    return NULL;
	}

	/* Look up the Token Fixed Key */
	Ks = PK11_FindFixedKey(slot, CKM_SKIPJACK_WRAP, NULL, ss->pkcs11PinArg);
	PK11_FreeSlot(slot);
	if (Ks == NULL) {
	    PORT_Free(header);
	    return NULL;
	}

	/* unwrap the key with the TEK */
	item.data = inHeader->u.fortezza.key;
	item.len = sizeof(inHeader->u.fortezza.key);
	key = PK11_UnwrapSymKey(tek,CKM_SKIPJACK_WRAP,
                        NULL, &item, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);
	if (key == NULL) {
	    PORT_Free(header);
	    PK11_FreeSymKey(Ks);
	    return NULL;
	}

	/* rewrap with the local Ks */
	item.data = header->u.fortezza.key;
	item.len = sizeof(header->u.fortezza.key);
	rv = PK11_WrapSymKey(CKM_SKIPJACK_WRAP, NULL, Ks, key, &item);
	PK11_FreeSymKey(Ks);
	PK11_FreeSymKey(key);
	if (rv != SECSuccess) {
	    PORT_Free(header);
	    return NULL;
	}
    
	/* copy our local serial number into header */
	for (i=0; i < sizeof(header->u.fortezza.serial); i++) {
	    header->u.fortezza.serial[i] = 
		(fromHex(info.serialNumber[i*2]) << 4) 	|
					fromHex(info.serialNumber[i*2 + 1]);
	}
	break;
    case PRE_FIXED_STREAM:
	/* not implemented yet */
	PORT_Free(header);
	return NULL;
    }
    
    return(header);
}

/*
 * this one needs to allocate space and work for RSA & FIXED key files as well
 */
PEHeader *SSL_PreencryptedFileToStream(PRFileDesc *fd, PEHeader *header, 
							int *headerSize)
{
    PK11SymKey *key, *tek, *Ks;
    sslSocket *ss;
    PK11SlotInfo *slot;
    SECStatus rv;
    SECItem item;
    
    *headerSize = 0; /* hack */
 
    if (fd == NULL) {
        /* XXX set an error */
        return NULL;
    }
    
    ss = ssl_FindSocket(fd);
    if (ss == NULL) {
        return NULL;
    }
    
    PORT_Assert(ss->ssl3 != NULL);
    if (ss->ssl3 == NULL) {
	return NULL;
    }

    /* get the kea context from the session */
    tek = ss->ssl3->fortezza.tek;
    if (tek == NULL) {
	return NULL;
    }

    slot = PK11_GetSlotFromKey(tek);
    if (slot == NULL) return NULL;
    Ks = PK11_FindFixedKey(slot, CKM_SKIPJACK_WRAP, NULL, PK11_GetWindow(tek));
    PK11_FreeSlot(slot);
    if (Ks == NULL) return NULL;


    /* unwrap with the local Ks */
    item.data = header->u.fortezza.key;
    item.len = sizeof(header->u.fortezza.key);
    /* rewrap the key with the TEK */
    key = PK11_UnwrapSymKey(Ks,CKM_SKIPJACK_WRAP,
                        NULL, &item, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);
    if (key == NULL) {
        PK11_FreeSymKey(Ks);
	return NULL;
    }

    rv = PK11_WrapSymKey(CKM_SKIPJACK_WRAP, NULL, tek, key, &item);
    PK11_FreeSymKey(Ks);
    PK11_FreeSymKey(key);
    if (rv != SECSuccess) {
	return NULL;
    }
    
    /* copy over our local serial number */
    PORT_Memset(header->u.fortezza.serial,0,sizeof(header->u.fortezza.serial));
    
    /* change type to stream */
    PutInt2(header->type, PRE_FORTEZZA_STREAM);
    
    return(header);
}


