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
 *
 * This file implements PKCS 11 for FORTEZZA using MACI
 *   drivers.  Except for the Mac.  They only have CI libraries, so
 *   that's what we use there.
 *
 *   For more information about PKCS 11 See PKCS 11 Token Inteface Standard.
 *
 *   This implementations queries the MACI to figure out how 
 *    many slots exist on a given system.  There is an artificial boundary
 *    of 32 slots, because allocating slots dynamically caused memory
 *    problems in the client when first this module was first developed.
 *    Some how the heap was being corrupted and we couldn't find out where.
 *    Subsequent attempts to allocate dynamic memory caused no problem.
 *
 *   In this implementation, session objects are only visible to the session
 *   that created or generated them.
 */

#include "fpkmem.h"
#include "seccomon.h"
#include "fpkcs11.h"
#include "fpkcs11i.h"
#include "cryptint.h"
#include "pk11func.h"
#include "fortsock.h"
#include "fmutex.h"
#ifdef notdef
#include <ctype.h>
#include <stdio.h>
#endif
#ifdef XP_MAC 
#ifndef __POWERPC__
#include <A4Stuff.h>
#endif
/* This is not a 4.0 project, so I can't depend on
 * 4.0 defines, so instead I depend on CodeWarrior 
 * defines.  I define XP_MAC in fpkmem.h
 */
#if __POWERPC__
#elif __CFM68K__
#else
/* These include are taken fromn npmac.cpp which are used
 * by the plugin group to properly set-up a plug-in for 
 * dynamic loading on 68K.
 */

#include <Quickdraw.h>

/*
** The Mixed Mode procInfos defined in npupp.h assume Think C-
** style calling conventions.  These conventions are used by
** Metrowerks with the exception of pointer return types, which
** in Metrowerks 68K are returned in A0, instead of the standard
** D0. Thus, since NPN_MemAlloc and NPN_UserAgent return pointers,
** Mixed Mode will return the values to a 68K plugin in D0, but 
** a 68K plugin compiled by Metrowerks will expect the result in
** A0.  The following pragma forces Metrowerks to use D0 instead.
*/
#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_D0
#endif
#endif

#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_A0
#endif
#endif

/* The following fix for static initializers fixes a previous
** incompatibility with some parts of PowerPlant.
*/
#ifdef __MWERKS__
#ifdef __cplusplus
    extern "C" {
#endif
#ifndef powerc
    extern void	__InitCode__(void);
#else
    extern void __sinit(void);
#endif
    extern void	__destroy_global_chain(void);
#ifdef __cplusplus
    }
#endif /* __cplusplus */
#endif /* __MWERKS__ */

#endif
#endif


typedef struct {
   unsigned char *data;
   int len;
} CertItem;


/*
 * ******************** Static data *******************************
 */

/* The next three strings must be exactly 32 characters long */
static char *manufacturerID      = "Netscape Communications Corp    ";
static char *libraryDescription  = "Communicator Fortezza Crypto Svc";

typedef enum {DSA_KEY, KEA_KEY, V1_KEY, INVALID_KEY } PrivKeyType;

static PK11Slot           fort11_slot[NUM_SLOTS];
static FortezzaSocket     fortezzaSockets[NUM_SLOTS];
static PRBool             init = PR_FALSE;
static CK_ULONG           kNumSockets    = 0;

#define __PASTE(x,y)    x##y
 
 
#undef CK_FUNC
#undef CK_EXTERN
#undef CK_NEED_ARG_LIST
#undef _CK_RV

#define fort11_attr_expand(ap) (ap)->type,(ap)->pValue,(ap)->ulValueLen
#define fort11_SlotFromSession  pk11_SlotFromSession
#define fort11_isToken          pk11_isToken

static CK_FUNCTION_LIST fort11_funcList = {
    { 2, 1 },
 
#undef CK_FUNC
#undef CK_EXTERN
#undef CK_NEED_ARG_LIST
#undef _CK_RV
 
#define CK_EXTERN
#define CK_FUNC(name) name,
#define _CK_RV
 
#include "fpkcs11f.h"
 
};
 
#undef CK_FUNC
#undef CK_EXTERN
#undef _CK_RV
 
 
#undef __PASTE
#undef pk11_SlotFromSessionHandle
#undef pk11_SlotFromID

#define MAJOR_VERSION_MASK 0xFF00
#define MINOR_VERSION_MASK 0x00FF

/* Mechanisms */
struct mechanismList {
    CK_MECHANISM_TYPE	type;
    CK_MECHANISM_INFO	domestic;
    PRBool              privkey; 
};

static struct mechanismList mechanisms[] = {
     {CKM_DSA,              {512,1024,CKF_SIGN},                 PR_TRUE},
     {CKM_SKIPJACK_KEY_GEN, {92, 92, CKF_GENERATE},              PR_TRUE},
     {CKM_SKIPJACK_CBC64,   {92, 92, CKF_ENCRYPT | CKF_DECRYPT}, PR_TRUE},
     {CKM_SKIPJACK_WRAP,    {92, 92, CKF_WRAP},                  PR_TRUE},
     {CKM_KEA_KEY_DERIVE,   {128, 128, CKF_DERIVE},              PR_TRUE},
};
static CK_ULONG mechanismCount = sizeof(mechanisms)/sizeof(mechanisms[0]);

/*************Static function prototypes********************************/
static PRBool          fort11_isTrue(PK11Object *object,CK_ATTRIBUTE_TYPE type);
static void            fort11_FreeAttribute(PK11Attribute *attribute);
static void            fort11_DestroyAttribute(PK11Attribute *attribute);
static PK11Object*     fort11_NewObject(PK11Slot *slot);
static PK11FreeStatus  fort11_FreeObject(PK11Object *object);
static CK_RV           fort11_AddAttributeType(PK11Object *object,
					     CK_ATTRIBUTE_TYPE type,
					     void *valPtr,
					     CK_ULONG length);
static void            fort11_AddSlotObject(PK11Slot *slot, PK11Object *object);
static PK11Attribute*  fort11_FindAttribute(PK11Object *object,
					  CK_ATTRIBUTE_TYPE type);
static PK11Attribute*  fort11_NewAttribute(CK_ATTRIBUTE_TYPE type, 
					 CK_VOID_PTR value, CK_ULONG len);
static void            fort11_DeleteAttributeType(PK11Object *object,
						CK_ATTRIBUTE_TYPE type);
static void            fort11_AddAttribute(PK11Object *object,
					 PK11Attribute *attribute);
static void            fort11_AddObject(PK11Session *session, 
				      PK11Object *object);
static PK11Object *    fort11_ObjectFromHandle(CK_OBJECT_HANDLE handle, 
					     PK11Session *session);
static void            fort11_DeleteObject(PK11Session *session,PK11Object *object);
static CK_RV           fort11_DestroyObject(PK11Object *object);
void   fort11_FreeSession(PK11Session *session);

#define FIRST_SLOT_SESS_ID   0x00000100L
#define ADD_NEXT_SESS_ID     0x00000100L
#define SLOT_MASK            0x000000FFL

#define FAILED CKR_FUNCTION_FAILED

static void
fort11_FreeFortezzaKey (void *inFortezzaKey) {
    RemoveKey ((FortezzaKey*) inFortezzaKey);
}

static void
fort11_DestroySlotObjects (PK11Slot *slot, PK11Session *session) {
    PK11Object *currObject, *nextObject, *oldObject;
    int i;

    for (i=0; i<HASH_SIZE; i++) {
      currObject = slot->tokObjects[i];
      slot->tokObjects[i] = NULL;
      do {
	  FMUTEX_Lock(slot->sessionLock);

	  if (currObject) {
	      nextObject = currObject->next;
	      FMUTEX_Lock(currObject->refLock);
	      currObject->refCount++;
	      FMUTEX_Unlock(currObject->refLock);
	      fort11_DeleteObject(session, currObject);
	  }
	  FMUTEX_Unlock(slot->sessionLock);
	  if (currObject) {
	      oldObject = currObject;
	      currObject = nextObject;
	      fort11_FreeObject(oldObject);
	  }
      } while (currObject != NULL);
    }
}

static void
fort11_TokenRemoved(PK11Slot *slot, PK11Session *session) {
    FortezzaSocket *socket = &fortezzaSockets[slot->slotID-1];

    LogoutFromSocket (socket);
    slot->isLoggedIn = PR_FALSE;
    if (session && session->notify) { 
        /*If no session pointer exists, lots of leaked memory*/
        session->notify (session->handle, CKN_SURRENDER,
			 session->appData);
	fort11_FreeSession(session); /* Release the reference held
				      * by the slot with the session
				      */
    }

    fort11_DestroySlotObjects(slot, session);
    fort11_FreeSession(session); /* Release the reference held
				  * by the slot with the session
				  */

    /* All keys will have been freed at this point so we can
     * NULL out this pointer
     */
    socket->keys = NULL;

}

PRBool
fort11_FortezzaIsUserCert(unsigned char * label) {

    if ( (!PORT_Memcmp(label, "KEAK", 4)) ||   /* v3 user certs */
         (!PORT_Memcmp(label, "DSA1", 4)) ||
	 (!PORT_Memcmp(label, "DSAI", 4)) ||
         (!PORT_Memcmp(label, "DSAO", 4)) ||
         (!PORT_Memcmp(label, "INKS", 4)) ||   /* v1 user certs */ 
         (!PORT_Memcmp(label, "INKX", 4)) ||
         (!PORT_Memcmp(label, "ONKS", 4)) ||
         (!PORT_Memcmp(label, "ONKX", 4)) ||
         (!PORT_Memcmp(label, "3IXS", 4)) ||   /* old v3 user certs */
         (!PORT_Memcmp(label, "3OXS", 4)) ||
         (!PORT_Memcmp(label, "3IKX", 4)) ) {  

         return PR_TRUE;

    } else { return PR_FALSE; }
    
}

static PRBool
fort11_FortezzaIsACert(unsigned char * label) {
    if (label == NULL) return PR_FALSE;

    if ( (!PORT_Memcmp(label, "DSA1", 4)) ||   /* v3 certs */
         (!PORT_Memcmp(label, "DSAI", 4)) ||
	 (!PORT_Memcmp(label, "DSAO", 4)) || 
         (!PORT_Memcmp(label, "DSAX", 4)) ||
         (!PORT_Memcmp(label, "KEAK", 4)) ||
         (!PORT_Memcmp(label, "KEAX", 4)) ||
         (!PORT_Memcmp(label, "CAX1", 4)) ||
         (!PORT_Memcmp(label, "PCA1", 4)) ||
         (!PORT_Memcmp(label, "PAA1", 4)) ||
         (!PORT_Memcmp(label, "ICA1", 4)) ||

         (!PORT_Memcmp(label, "3IXS", 4)) ||   /* old v3 certs */    
         (!PORT_Memcmp(label, "3OXS", 4)) ||    
         (!PORT_Memcmp(label, "3CAX", 4)) ||
         (!PORT_Memcmp(label, "3IKX", 4)) ||
         (!PORT_Memcmp(label, "3PCA", 4)) ||
         (!PORT_Memcmp(label, "3PAA", 4)) ||
         (!PORT_Memcmp(label, "3ICA", 4)) ||

         (!PORT_Memcmp(label, "INKS", 4)) ||   /* v1 certs */    
         (!PORT_Memcmp(label, "INKX", 4)) ||
         (!PORT_Memcmp(label, "ONKS", 4)) ||
         (!PORT_Memcmp(label, "ONKX", 4)) ||
         (!PORT_Memcmp(label, "RRXX", 4)) ||
         (!PORT_Memcmp(label, "RTXX", 4)) ||
         (!PORT_Memcmp(label, "LAXX", 4)) ) {  

         return PR_TRUE;

    } 

    return PR_FALSE; 
}
 
static 
int fort11_cert_length(unsigned char *buf, int length) {
    unsigned char tag;
    int used_length= 0;
    int data_length;

    tag = buf[used_length++];

    /* blow out when we come to the end */
    if (tag == 0) {
	return 0;
    }

    data_length = buf[used_length++];

    if (data_length&0x80) {
	int  len_count = data_length & 0x7f;

	data_length = 0;

	while (len_count-- > 0) {
	    data_length = (data_length << 8) | buf[used_length++];
	} 
    }

    if (data_length > (length-used_length) ) {
	return length;
    }

    return (data_length + used_length);	
}

unsigned char *fort11_data_start(unsigned char *buf, int length, 
				 int *data_length, PRBool includeTag) {
    unsigned char tag;
    int used_length= 0;

    tag = buf[used_length++];

    /* blow out when we come to the end */
    if (tag == 0) {
	return NULL;
    }

    *data_length = buf[used_length++];

    if (*data_length&0x80) {
	int  len_count = *data_length & 0x7f;

	*data_length = 0;

	while (len_count-- > 0) {
	    *data_length = (*data_length << 8) | buf[used_length++];
	} 
    }

    if (*data_length > (length-used_length) ) {
	*data_length = length-used_length;
	return NULL;
    }
    if (includeTag) *data_length += used_length;

    return (buf + (includeTag ? 0 : used_length));	
}

int
fort11_GetCertFields(unsigned char *cert,int cert_length,CertItem *issuer,
		     CertItem *serial,CertItem *subject)
{
	unsigned char *buf;
	int buf_length;
	unsigned char *date;
	int datelen;

	/* get past the signature wrap */
	buf = fort11_data_start(cert,cert_length,&buf_length,PR_FALSE);
	if (buf == NULL) return FAILED;
	/* get into the raw cert data */
	buf = fort11_data_start(buf,buf_length,&buf_length,PR_FALSE);
	if (buf == NULL) return FAILED;
	/* skip past any optional version number */
        if ((buf[0] & 0xa0) == 0xa0) {
            date = fort11_data_start(buf,buf_length,&datelen,PR_FALSE);
            if (date == NULL) return FAILED;
            buf_length -= (date-buf) + datelen;
            buf = date + datelen;
        }
        /* serial number */
	serial->data = fort11_data_start(buf,buf_length,&serial->len,PR_FALSE);
	if (serial->data == NULL) return FAILED;
	buf_length -= (serial->data-buf) + serial->len;
	buf = serial->data + serial->len;
	/* skip the OID */
	date = fort11_data_start(buf,buf_length,&datelen,PR_FALSE);
	if (date == NULL) return FAILED;
	buf_length -= (date-buf) + datelen;
	buf = date + datelen;
	/* issuer */
	issuer->data = fort11_data_start(buf,buf_length,&issuer->len,PR_TRUE);
	if (issuer->data == NULL) return FAILED;
	buf_length -= (issuer->data-buf) + issuer->len;
	buf = issuer->data + issuer->len;
	/* skip the date */
	date = fort11_data_start(buf,buf_length,&datelen,PR_FALSE);
	if (date == NULL) return FAILED;
	buf_length -= (date-buf) + datelen;
	buf = date + datelen;
	/*subject */
	subject->data=fort11_data_start(buf,buf_length,&subject->len,PR_TRUE);
	if (subject->data == NULL) return FAILED;
	buf_length -= (subject->data-buf) + subject->len;
	buf = subject->data +subject->len;
	/*subject */
	return CKR_OK;
}

/* quick tohex function to get rid of scanf */
static
int fort11_tohex(char *s) {
    int val = 0;

    for(;*s;s++) {
	if ((*s >= '0') && (*s <= '9')) {
	    val = (val << 4) + (*s - '0');
	    continue;
	} else if ((*s >= 'a') && (*s <= 'f')) {
	    val = (val << 4) + (*s - 'a') + 10;
	    continue;
	} else if ((*s >= 'A') && (*s <= 'F')) {
	    val = (val << 4) + (*s - 'A') + 10;
	    continue;
	}
	break;
    }
    return val;
}

/* only should be called for V3 KEA cert labels. */

static int
fort11_GetSibling(CI_CERT_STR label)  {

	int value = 0;
        char s[3];

        label +=4;

        strcpy(s,"00");
        memcpy(s, label, 2);
	value = fort11_tohex(s);
        
	/*  sibling of 255 means no sibling */
        if (value == 255) {
	    value = -1;
	}

	return value;
}


static PrivKeyType
fort11_GetKeyType(CI_CERT_STR label) { 
    if (label == NULL) return INVALID_KEY;

    if ( (!PORT_Memcmp(label, "DSA1", 4)) ||   /* v3 certs */ 
         (!PORT_Memcmp(label, "DSAI", 4)) ||
	 (!PORT_Memcmp(label, "DSAO", 4)) ||
         (!PORT_Memcmp(label, "3IXS", 4)) ||   /* old v3 certs */ 
         (!PORT_Memcmp(label, "3OXS", 4)) ) {

        return DSA_KEY;
    }


    if ( (!PORT_Memcmp(label, "KEAK", 4)) || 
         (!PORT_Memcmp(label, "3IKX", 4)) ) {
        return KEA_KEY;
    }
 
    if ( (!PORT_Memcmp(label, "INKS", 4)) ||  /* V1 Certs*/
         (!PORT_Memcmp(label, "INKX", 4)) ||
         (!PORT_Memcmp(label, "ONKS", 4)) ||
         (!PORT_Memcmp(label, "ONKX", 4)) ||  
         (!PORT_Memcmp(label, "RRXX", 4)) ||
         (!PORT_Memcmp(label, "RTXX", 4)) ||
         (!PORT_Memcmp(label, "LAXX", 4)) ) { 

         return V1_KEY;
    }

    return INVALID_KEY;
} 

static CK_RV
fort11_ConvertToDSAKey(PK11Object *privateKey, PK11Slot *slot) {
    CK_KEY_TYPE     key_type  = CKK_DSA;
    CK_BBOOL        cktrue    = TRUE;
    CK_BBOOL        ckfalse   = FALSE;
    CK_OBJECT_CLASS privClass = CKO_PRIVATE_KEY;
    CK_CHAR         label[]   = "A DSA Private Key";


    /* Fill in the common Default values */
    if (fort11_AddAttributeType(privateKey,CKA_START_DATE, NULL, 0) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey,CKA_END_DATE, NULL, 0) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey,CKA_SUBJECT, NULL, 0) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_CLASS, &privClass, 
			      sizeof (CK_OBJECT_CLASS)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_KEY_TYPE, &key_type, 
			      sizeof(CK_KEY_TYPE)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType (privateKey, CKA_TOKEN, &cktrue, 
			      sizeof (CK_BBOOL)) != CKR_OK) {
       return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType (privateKey, CKA_LABEL, label,
			       PORT_Strlen((char*)label)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_SENSITIVE, &cktrue, 
			      sizeof (CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_SIGN, &cktrue, 
			      sizeof (CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(privateKey, CKA_DERIVE, &cktrue,
			      sizeof(cktrue)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_LOCAL, &ckfalse,
			      sizeof(ckfalse)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_DECRYPT, &ckfalse,
			      sizeof(ckfalse)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_SIGN_RECOVER, &ckfalse,
			      sizeof(ckfalse)) != CKR_OK) {
 	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_UNWRAP, &ckfalse,
			      sizeof(ckfalse)) != CKR_OK) {
 	return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_EXTRACTABLE, &ckfalse,
			      sizeof(ckfalse))  != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_ALWAYS_SENSITIVE, &cktrue,
			      sizeof(cktrue)) != CKR_OK) {
       return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_NEVER_EXTRACTABLE, &cktrue,
			      sizeof(ckfalse)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_PRIME, NULL, 0) != CKR_OK){
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_SUBPRIME, NULL, 0) != CKR_OK){
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_BASE, NULL, 0) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_VALUE, NULL, 0) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_PRIVATE, &cktrue,
			      sizeof(cktrue)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_MODIFIABLE,&ckfalse,
			      sizeof(ckfalse)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }

    FMUTEX_Lock(slot->objectLock);
    privateKey->handle = slot->tokenIDCount++;
    privateKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);
    privateKey->objclass = privClass;
    privateKey->slot = slot;
    privateKey->inDB = PR_TRUE;


    return CKR_OK;
}

static int
fort11_LoadRootPAAKey(PK11Slot *slot, PK11Session *session) {
    CK_OBJECT_CLASS theClass   = CKO_SECRET_KEY;
    int              id      = 0;
    CK_BBOOL         True    = TRUE;
    CK_BBOOL          False   = FALSE;
    CK_CHAR          label[] = "Trusted Root PAA Key";
    PK11Object      *rootKey;
    FortezzaKey     *newKey;
    FortezzaSocket *socket  = &fortezzaSockets[slot->slotID-1];
    
    /*Don't know the key type.  Does is matter?*/

    rootKey = fort11_NewObject(slot);

    if (rootKey == NULL) {
        return CKR_HOST_MEMORY;
    }

    if (fort11_AddAttributeType(rootKey, CKA_CLASS, &theClass,
			      sizeof(theClass)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey, CKA_TOKEN, &True,
			      sizeof(True)) != CKR_OK) {        
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey, CKA_LABEL, label,
			      sizeof(label)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey, CKA_PRIVATE, &True,
			      sizeof (True)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey,CKA_MODIFIABLE, &False, 
			      sizeof(False)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }
    
    if (fort11_AddAttributeType(rootKey, CKA_ID, &id,
			      sizeof(int)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey, CKA_DERIVE, &True,
			      sizeof(True)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    if (fort11_AddAttributeType(rootKey, CKA_SENSITIVE, &True,
			      sizeof(True)) != CKR_OK) {
	return CKR_GENERAL_ERROR;
    }

    FMUTEX_Lock(slot->objectLock);
    rootKey->handle = slot->tokenIDCount++;
    rootKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);

    rootKey->objclass = theClass;
    rootKey->slot = slot;
    rootKey->inDB = PR_TRUE;

    newKey = NewFortezzaKey(socket, Ks, NULL, 0);
    if (newKey == NULL) {
        fort11_FreeObject(rootKey);
        return CKR_HOST_MEMORY;
    }

    rootKey->objectInfo = (void*)newKey;
    rootKey->infoFree   = fort11_FreeFortezzaKey;
    fort11_AddObject(session, rootKey);

    return CKR_OK;
}

static CK_RV
fort11_ConvertToKEAKey (PK11Object *privateKey, PK11Slot *slot) {
    CK_OBJECT_CLASS theClass   = CKO_PRIVATE_KEY;
    CK_KEY_TYPE     keyType = CKK_KEA;
    CK_CHAR         label[] = "A KEA private key Object";
    CK_BBOOL        True    = TRUE;
    CK_BBOOL        False   = FALSE;

    if (fort11_AddAttributeType(privateKey, CKA_CLASS, &theClass,
			      sizeof (CK_OBJECT_CLASS)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_KEY_TYPE, &keyType,
			      sizeof (CK_KEY_TYPE)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_TOKEN, &True, 
			      sizeof(CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType (privateKey, CKA_LABEL, label,
			       PORT_Strlen((char*)label)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType (privateKey, CKA_SENSITIVE, 
			       &True, sizeof(CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }    
    if (fort11_AddAttributeType (privateKey, CKA_DERIVE, 
			       &True, sizeof(CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_PRIVATE, &True,
			      sizeof(True)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_START_DATE, NULL, 0) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    } 
    if (fort11_AddAttributeType(privateKey, CKA_END_DATE, NULL, 0) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }
    if (fort11_AddAttributeType(privateKey, CKA_LOCAL, &False,
			      sizeof(False)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }

    FMUTEX_Lock(slot->objectLock);
    privateKey->handle = slot->tokenIDCount++;
    privateKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);
    privateKey->objclass = theClass;
    privateKey->slot = slot;
    privateKey->inDB = PR_TRUE;

    return CKR_OK;
}

static CK_RV
fort11_ConvertToV1Key (PK11Object* privateKey, PK11Slot *slot) {
    CK_RV    rv;
    CK_BBOOL True = TRUE;

    rv = fort11_ConvertToDSAKey(privateKey, slot);
    if (rv != CKR_OK) {
        return rv;
    }

    if (fort11_AddAttributeType(privateKey, CKA_DERIVE, &True, 
			      sizeof (CK_BBOOL)) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }

    return CKR_OK;
}

static CK_RV
fort11_NewPrivateKey(PK11Object *privKeyObject, PK11Slot *slot,CI_PERSON currPerson) {
  PrivKeyType keyType = fort11_GetKeyType(currPerson.CertLabel);
  CK_RV       rv;

  switch (keyType) {
  case DSA_KEY:
    rv = fort11_ConvertToDSAKey(privKeyObject, slot);
    break;
  case KEA_KEY:
    rv = fort11_ConvertToKEAKey(privKeyObject, slot);
    break;
  case V1_KEY:
    rv = fort11_ConvertToV1Key(privKeyObject, slot);
    break;
  default:
    rv = CKR_GENERAL_ERROR;
    break;
  }
  return rv;
}


PRBool
fort11_LoadCertObjectForSearch(CI_PERSON currPerson, PK11Slot *slot, 
			PK11Session *session, CI_PERSON *pers_array) {
    PK11Object          *certObject, *privKeyObject;
    PK11Attribute       *attribute, *newAttribute;
    int                  ci_rv;
    CI_CERTIFICATE       cert;
    CK_OBJECT_CLASS      certClass = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE  certType  = CKC_X_509;
    CK_BBOOL             cktrue    = TRUE;
    CK_BBOOL             ckfalse   = FALSE;
    CertItem             issuer, serial, subject;
    int			 certSize;
    char                 nickname[50];
    char                *cursor; 
    PrivKeyType		 priv_key;
    int			 sibling;
  

    certObject = fort11_NewObject(slot);
    if (certObject == NULL)
        return PR_FALSE;

    ci_rv = MACI_GetCertificate (fortezzaSockets[slot->slotID-1].maciSession,
				 currPerson.CertificateIndex, cert);
    if (ci_rv != CI_OK){
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }

    ci_rv = fort11_GetCertFields(cert,CI_CERT_SIZE,&issuer,&serial,&subject);

    if (ci_rv != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }

    if (fort11_AddAttributeType(certObject, CKA_CLASS, &certClass,
			      sizeof (CK_OBJECT_CLASS)) != CKR_OK) {
        fort11_FreeObject (certObject);
	return PR_FALSE;
    }
    if (fort11_AddAttributeType(certObject, CKA_TOKEN, &cktrue,
			      sizeof (CK_BBOOL)) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }
    if (fort11_AddAttributeType(certObject, CKA_PRIVATE, &ckfalse,
			      sizeof (CK_BBOOL)) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }

    
    /* check if the label represents a KEA key. if so, the
       nickname should be made the same as the corresponding DSA
       sibling cert. */
        
    priv_key = fort11_GetKeyType(currPerson.CertLabel);

    if (priv_key == KEA_KEY) {
	sibling = fort11_GetSibling(currPerson.CertLabel);

	/* check for failure of fort11_GetSibling.  also check that
	   the sibling is not zero.  */

	if (sibling > 0) {
	    /* assign the KEA cert label to be the same as the
	       sibling DSA label */

	    sprintf (nickname, "%s", &pers_array[sibling-1].CertLabel[8] );
	} else {
	    sprintf (nickname, "%s", &currPerson.CertLabel[8]);
	}
    } else {
        sprintf (nickname, "%s", &currPerson.CertLabel[8]);
    }

    cursor = nickname+PORT_Strlen(nickname)-1;
    while ((*cursor) == ' ') {
        cursor--;
    }
    cursor[1] = '\0';
    if (fort11_AddAttributeType(certObject, CKA_LABEL, nickname,
			      PORT_Strlen(nickname)) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }

 

    if (fort11_AddAttributeType(certObject, CKA_CERTIFICATE_TYPE, &certType,
			      sizeof(CK_CERTIFICATE_TYPE)) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }
    certSize = fort11_cert_length(cert,CI_CERT_SIZE);
    if (fort11_AddAttributeType (certObject, CKA_VALUE, cert, certSize)
	                                                        != CI_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }
    if (fort11_AddAttributeType(certObject, CKA_ISSUER, issuer.data,
			  issuer.len) != CKR_OK) {
        fort11_FreeObject (certObject);
	return PR_FALSE;
    }
    if (fort11_AddAttributeType(certObject, CKA_SUBJECT, subject.data,
			      subject.len) != CKR_OK) {
        fort11_FreeObject (certObject);
	return PR_FALSE;
    }
    if (fort11_AddAttributeType(certObject, CKA_SERIAL_NUMBER,
			      serial.data, serial.len) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }
    /*Change this to a byte array later*/
    if (fort11_AddAttributeType(certObject, CKA_ID, 
				&currPerson.CertificateIndex,
				sizeof(int)) != CKR_OK) {
        fort11_FreeObject(certObject);
	return PR_FALSE;
    }
    certObject->objectInfo = NULL;
    certObject->infoFree   = NULL;

    certObject->objclass = certClass;
    certObject->slot  = slot;
    certObject->inDB  = PR_TRUE;

    FMUTEX_Lock(slot->objectLock);
      
    certObject->handle  = slot->tokenIDCount++;
    certObject->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_CERT);

    FMUTEX_Unlock(slot->objectLock);

    if (fort11_FortezzaIsUserCert (currPerson.CertLabel)) {
        privKeyObject = fort11_NewObject(slot);
        if (fort11_NewPrivateKey(privKeyObject, slot, currPerson) != CKR_OK) {
	    fort11_FreeObject(privKeyObject);
	    fort11_FreeObject(certObject);
	    return PR_FALSE;
	}
	if(fort11_AddAttributeType(privKeyObject,CKA_ID,
				 &currPerson.CertificateIndex,
				 sizeof(int)) != CKR_OK) {
	    fort11_FreeObject(privKeyObject);
	    fort11_FreeObject(certObject);
	    return PR_FALSE;
	}	    
	attribute = fort11_FindAttribute(certObject,CKA_SUBJECT);
	newAttribute=
	  fort11_NewAttribute(pk11_attr_expand(&attribute->attrib));
	fort11_FreeAttribute(attribute);
	if (newAttribute != NULL) {
	    fort11_DeleteAttributeType(privKeyObject,
				     CKA_SUBJECT);
	    fort11_AddAttribute(privKeyObject,
			      newAttribute);
	}
	fort11_AddObject (session, privKeyObject);
    }


    fort11_AddObject (session, certObject);

    
    return PR_TRUE;
}

#define TRUSTED_PAA "00000000Trusted Root PAA"

static int
fort11_BuildCertObjects(FortezzaSocket *currSocket, PK11Slot *slot, 
		       PK11Session *session) {
		       
    int i;
    CI_PERSON rootPAA;

    PORT_Memcpy (rootPAA.CertLabel, TRUSTED_PAA, 1+PORT_Strlen (TRUSTED_PAA));
    rootPAA.CertificateIndex = 0;

    if (!fort11_LoadCertObjectForSearch(rootPAA, slot, session, 
				 currSocket->personalityList)) {
        return CKR_GENERAL_ERROR;
    }

    if (fort11_LoadRootPAAKey(slot, session) != CKR_OK) {
        return CKR_GENERAL_ERROR;
    }

    for (i=0 ; i < currSocket->numPersonalities; i++) {
        if (fort11_FortezzaIsACert (currSocket->personalityList[i].CertLabel)){
	    if (!fort11_LoadCertObjectForSearch(currSocket->personalityList[i],
						slot, session, 
						currSocket->personalityList)){
	        return CKR_GENERAL_ERROR;
	}
      }
    }

    return CKR_OK;
}

PK11Slot*
fort11_SlotFromSessionHandle(CK_SESSION_HANDLE inHandle) {
 CK_SESSION_HANDLE whichSlot = inHandle & SLOT_MASK;

 if (whichSlot >= kNumSockets) return NULL_PTR;

 return &fort11_slot[whichSlot];
}

PK11Slot* 
fort11_SlotFromID (CK_SLOT_ID inSlotID) {
  if (inSlotID == 0 || inSlotID > kNumSockets)
    return NULL;

  return &fort11_slot[inSlotID-1];
}

CK_ULONG fort11_firstSessionID (int inSlotNum) {
        return (CK_ULONG)(inSlotNum);
}

/*
 * Utility to convert passed in PIN to a CI_PIN
 */
void fort11_convertToCIPin (CI_PIN ciPin,CK_CHAR_PTR pPin, CK_ULONG ulLen) {
  unsigned long i;

  for (i=0; i<ulLen; i++) {
    ciPin[i] = pPin[i];
  }
  ciPin[ulLen] = '\0';
}


/*
 * return true if object has attribute
 */
static PRBool
fort11_hasAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type) {
    PK11Attribute *attribute;

    FMUTEX_Lock(object->attributeLock);
    pk11queue_find(attribute,type,object->head,HASH_SIZE);
    FMUTEX_Unlock(object->attributeLock);

    return (PRBool)(attribute != NULL);
}

/*
 * create a new attribute with type, value, and length. Space is allocated
 * to hold value.
 */
static PK11Attribute *
fort11_NewAttribute(CK_ATTRIBUTE_TYPE type, CK_VOID_PTR value, CK_ULONG len) {
    PK11Attribute *attribute;
    CK_RV mrv;

    attribute = (PK11Attribute*)PORT_Alloc(sizeof(PK11Attribute));
    if (attribute == NULL) return NULL;

    attribute->attrib.type = type;
    if (value) {
	attribute->attrib.pValue = (CK_VOID_PTR)PORT_Alloc(len);
	if (attribute->attrib.pValue == NULL) {
	    PORT_Free(attribute);
	    return NULL;
	}
	PORT_Memcpy(attribute->attrib.pValue,value,len);
	attribute->attrib.ulValueLen = len;
    } else {
	attribute->attrib.pValue = NULL;
	attribute->attrib.ulValueLen = 0;
    }
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
    attribute->refCount = 1;
    if (FMUTEX_MutexEnabled()) {
        mrv = FMUTEX_Create (&attribute->refLock);
	if (mrv != CKR_OK) {
	    if (attribute->attrib.pValue) PORT_Free(attribute->attrib.pValue);
	    PORT_Free(attribute);
	    return NULL;
	}
    } else {
        attribute->refLock = NULL;
    }

    return attribute;
}

/*
 * add an attribute to an object
 */
static
void fort11_AddAttribute(PK11Object *object,PK11Attribute *attribute) {
    FMUTEX_Lock (object->attributeLock);
    pk11queue_add(attribute,attribute->handle,object->head,HASH_SIZE);
    FMUTEX_Unlock(object->attributeLock);
}

static CK_RV
fort11_AddAttributeType(PK11Object *object,CK_ATTRIBUTE_TYPE type,void *valPtr,
							CK_ULONG length) {
    PK11Attribute *attribute;
    attribute = fort11_NewAttribute(type,valPtr,length);
    if (attribute == NULL) { return CKR_HOST_MEMORY; }
    fort11_AddAttribute(object,attribute);
    return CKR_OK;
}



/* Make sure a given attribute exists. If it doesn't, initialize it to
 * value and len
 */
static CK_RV
fort11_forceAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type,void *value,
							unsigned int len) {
    if ( !fort11_hasAttribute(object, type)) {
	return fort11_AddAttributeType(object,type,value,len);
    }
    return CKR_OK;
}

/*
 * look up and attribute structure from a type and Object structure.
 * The returned attribute is referenced and needs to be freed when 
 * it is no longer needed.
 */
static PK11Attribute *
fort11_FindAttribute(PK11Object *object,CK_ATTRIBUTE_TYPE type) {
    PK11Attribute *attribute;

    FMUTEX_Lock(object->attributeLock);
    pk11queue_find(attribute,type,object->head,HASH_SIZE);
    if (attribute) {
	/* atomic increment would be nice here */
        FMUTEX_Lock(attribute->refLock);
	attribute->refCount++;
	FMUTEX_Unlock(attribute->refLock);
    }
    FMUTEX_Unlock(object->attributeLock);

    return(attribute);
}

/*
 * this is only valid for CK_BBOOL type attributes. Return the state
 * of that attribute.
 */
static PRBool
fort11_isTrue(PK11Object *object,CK_ATTRIBUTE_TYPE type) {
    PK11Attribute *attribute;
    PRBool tok = PR_FALSE;

    attribute=fort11_FindAttribute(object,type);
    if (attribute == NULL) { return PR_FALSE; }
    tok = (PRBool)(*(CK_BBOOL *)attribute->attrib.pValue);
    fort11_FreeAttribute(attribute);

    return tok;
}

/*
 * add an object to a slot and session queue
 */
static
void fort11_AddSlotObject(PK11Slot *slot, PK11Object *object) {
    FMUTEX_Lock(slot->objectLock);
    pk11queue_add(object,object->handle,slot->tokObjects,HASH_SIZE);
    FMUTEX_Unlock(slot->objectLock);
}

static
void fort11_AddObject(PK11Session *session, PK11Object *object) {
    PK11Slot *slot = fort11_SlotFromSession(session);

    if (!fort11_isToken(object->handle)) {
        FMUTEX_Lock(session->objectLock);
	pk11queue_add(&object->sessionList,0,session->objects,0);
	FMUTEX_Unlock(session->objectLock);
    }
    fort11_AddSlotObject(slot,object);
} 

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static CK_RV
fort11_DestroyObject(PK11Object *object) {
    int i;
    CK_RV crv = CKR_OK;
/*    PORT_Assert(object->refCount == 0);*/

    if (object->label) PORT_Free(object->label);

    /* clean out the attributes */
    /* since no one is referencing us, it's safe to walk the chain
     * without a lock */
    for (i=0; i < HASH_SIZE; i++) {
	PK11Attribute *ap,*next;
	for (ap = object->head[i]; ap != NULL; ap = next) {
	    next = ap->next;
	    /* paranoia */
	    ap->next = ap->prev = NULL;
	    fort11_FreeAttribute(ap);
	}
	object->head[i] = NULL;
    }
    FMUTEX_Destroy(object->attributeLock);
    FMUTEX_Destroy(object->refLock);
    if (object->objectInfo) {
	(*object->infoFree)(object->objectInfo);
    }
    PORT_Free(object);
    return crv;
}


/*
 * release a reference to an attribute structure
 */
static void
fort11_FreeAttribute(PK11Attribute *attribute) {
    PRBool destroy = PR_FALSE;

    FMUTEX_Lock(attribute->refLock);
    if (attribute->refCount == 1) destroy = PR_TRUE;
    attribute->refCount--;
    FMUTEX_Unlock(attribute->refLock);

    if (destroy) fort11_DestroyAttribute(attribute);
}


/*
 * release a reference to an object handle
 */
static PK11FreeStatus
fort11_FreeObject(PK11Object *object) {
    PRBool destroy = PR_FALSE;
    CK_RV crv;

    FMUTEX_Lock(object->refLock);
    if (object->refCount == 1) destroy = PR_TRUE;
    object->refCount--;
    FMUTEX_Unlock(object->refLock);
      
    if (destroy) {
      crv = fort11_DestroyObject(object);
      if (crv != CKR_OK) {
	return PK11_DestroyFailure;
      } 
      return PK11_Destroyed;
    }
    return PK11_Busy;
}

static void
fort11_update_state(PK11Slot *slot,PK11Session *session) {
    if (slot->isLoggedIn) {
	if (slot->ssoLoggedIn) {
    	    session->info.state = CKS_RW_SO_FUNCTIONS;
	} else if (session->info.flags & CKF_RW_SESSION) {
    	    session->info.state = CKS_RW_USER_FUNCTIONS;
	} else {
    	    session->info.state = CKS_RO_USER_FUNCTIONS;
	}
    } else {
	if (session->info.flags & CKF_RW_SESSION) {
    	    session->info.state = CKS_RW_PUBLIC_SESSION;
	} else {
    	    session->info.state = CKS_RO_PUBLIC_SESSION;
	}
    }
}

/* update the state of all the sessions on a slot */
static void
fort11_update_all_states(PK11Slot *slot) {
    int i;
    PK11Session *session;

    for (i=0; i < SESSION_HASH_SIZE; i++) {
        FMUTEX_Lock(slot->sessionLock);
	for (session = slot->head[i]; session; session = session->next) {
	    fort11_update_state(slot,session);
	}
	FMUTEX_Unlock(slot->sessionLock);
    }
}


/*
 * Create a new object
 */
static PK11Object *
fort11_NewObject(PK11Slot *slot) {
    PK11Object *object;
    CK_RV mrv;
    int i;

    object = (PK11Object*)PORT_Alloc(sizeof(PK11Object));
    if (object == NULL) return NULL;

    object->handle = 0;
    object->next = object->prev = NULL;
    object->sessionList.next = NULL;
    object->sessionList.prev = NULL;
    object->sessionList.parent = object;
    object->inDB = PR_FALSE;
    object->label = NULL;
    object->refCount = 1;
    object->session = NULL;
    object->slot = slot;
    object->objclass = 0xffff;
    if (FMUTEX_MutexEnabled()) {
        mrv = FMUTEX_Create(&object->refLock);
	if (mrv != CKR_OK) {
	    PORT_Free(object);
	    return NULL;
	}
	mrv = FMUTEX_Create(&object->attributeLock);
	if (mrv != CKR_OK) {
	    FMUTEX_Destroy(object->refLock);
	    PORT_Free(object);
	    return NULL;
	}
    } else {
        object->attributeLock = NULL;
	object->refLock       = NULL;
    }   
    for (i=0; i < HASH_SIZE; i++) {
	object->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

/*
 * look up and object structure from a handle. OBJECT_Handles only make
 * sense in terms of a given session.  make a reference to that object
 * structure returned.
 */
static PK11Object * fort11_ObjectFromHandle(CK_OBJECT_HANDLE handle, 
					    PK11Session *session) {
    PK11Object **head;
    void *lock;
    PK11Slot *slot = fort11_SlotFromSession(session);
    PK11Object *object;

    /*
     * Token objects are stored in the slot. Session objects are stored
     * with the session.
     */
    head = slot->tokObjects;
    lock = slot->objectLock;

    FMUTEX_Lock(lock);
    pk11queue_find(object,handle,head,HASH_SIZE);
    if (object) {
        FMUTEX_Lock(object->refLock);
	object->refCount++;
	FMUTEX_Unlock(object->refLock);
    }
    FMUTEX_Unlock(lock);

    return(object);
}

/*
 * add an object to a slot andsession queue
 */
static
void fort11_DeleteObject(PK11Session *session, PK11Object *object) {
    PK11Slot *slot;
    
    if (session == NULL) 
        return;
    slot = fort11_SlotFromSession(session);
    if (!fort11_isToken(object->handle)) {
        FMUTEX_Lock(session->objectLock);
	pk11queue_delete(&object->sessionList,0,session->objects,0);
	FMUTEX_Unlock(session->objectLock);
    }
    FMUTEX_Lock(slot->objectLock);
    pk11queue_delete(object,object->handle,slot->tokObjects,HASH_SIZE);
    FMUTEX_Unlock(slot->objectLock);
    fort11_FreeObject(object);
}



/*
 * ******************** Search Utilities *******************************
 */

/* add an object to a search list */
CK_RV
fort11_AddToList(PK11ObjectListElement **list,PK11Object *object) {
     PK11ObjectListElement *newelem = 
	(PK11ObjectListElement *)PORT_Alloc(sizeof(PK11ObjectListElement));

     if (newelem == NULL) return CKR_HOST_MEMORY;

     newelem->next = *list;
     newelem->object = object;
     FMUTEX_Lock(object->refLock);
     object->refCount++;
     FMUTEX_Unlock(object->refLock);

    *list = newelem;
    return CKR_OK;
}


/*
 * free a single list element. Return the Next object in the list.
 */
PK11ObjectListElement *
fort11_FreeObjectListElement(PK11ObjectListElement *objectList) {
    PK11ObjectListElement *ol = objectList->next;

    fort11_FreeObject(objectList->object);
    PORT_Free(objectList);
    return ol;
}

/* free an entire object list */
void
fort11_FreeObjectList(PK11ObjectListElement *objectList) {
    PK11ObjectListElement *ol;

    for (ol= objectList; ol != NULL; ol = fort11_FreeObjectListElement(ol)) {}
}

/*
 * free a search structure
 */
void
fort11_FreeSearch(PK11SearchResults *search) {
    if (search->handles) {
	PORT_Free(search->handles);
    }
    PORT_Free(search);
}


/*
 * Free up all the memory associated with an attribute. Reference count
 * must be zero to call this.
 */
static void
fort11_DestroyAttribute(PK11Attribute *attribute) {
    /*PORT_Assert(attribute->refCount == 0);*/
    FMUTEX_Destroy(attribute->refLock);
    if (attribute->attrib.pValue) {
	 /* clear out the data in the attribute value... it may have been
	  * sensitive data */
	 PORT_Memset(attribute->attrib.pValue,0,attribute->attrib.ulValueLen);
	 PORT_Free(attribute->attrib.pValue);
    }
    PORT_Free(attribute);
}
        
/*
 * delete an attribute from an object
 */
static void
fort11_DeleteAttribute(PK11Object *object, PK11Attribute *attribute) {
    FMUTEX_Lock(object->attributeLock);
    if (attribute->next || attribute->prev) {
	pk11queue_delete(attribute,attribute->handle,
						object->head,HASH_SIZE);
    }
    FMUTEX_Unlock(object->attributeLock);
    fort11_FreeAttribute(attribute);
}

/*
 * decode when a particular attribute may be modified
 * 	PK11_NEVER: This attribute must be set at object creation time and
 *  can never be modified.
 *	PK11_ONCOPY: This attribute may be modified only when you copy the
 *  object.
 *	PK11_SENSITIVE: The CKA_SENSITIVE attribute can only be changed from
 *  FALSE to TRUE.
 *	PK11_ALWAYS: This attribute can always be modified.
 * Some attributes vary their modification type based on the class of the 
 *   object.
 */
PK11ModifyType
fort11_modifyType(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass) {
    /* if we don't know about it, user user defined, always allow modify */
    PK11ModifyType mtype = PK11_ALWAYS; 

    switch(type) {
    /* NEVER */
    case CKA_CLASS:
    case CKA_CERTIFICATE_TYPE:
    case CKA_KEY_TYPE:
    case CKA_MODULUS:
    case CKA_MODULUS_BITS:
    case CKA_PUBLIC_EXPONENT:
    case CKA_PRIVATE_EXPONENT:
    case CKA_PRIME:
    case CKA_SUBPRIME:
    case CKA_BASE:
    case CKA_PRIME_1:
    case CKA_PRIME_2:
    case CKA_EXPONENT_1:
    case CKA_EXPONENT_2:
    case CKA_COEFFICIENT:
    case CKA_VALUE_LEN:
	mtype = PK11_NEVER;
	break;

    /* ONCOPY */
    case CKA_TOKEN:
    case CKA_PRIVATE:
	mtype = PK11_ONCOPY;
	break;

    /* SENSITIVE */
    case CKA_SENSITIVE:
	mtype = PK11_SENSITIVE;
	break;

    /* ALWAYS */
    case CKA_LABEL:
    case CKA_APPLICATION:
    case CKA_ID:
    case CKA_SERIAL_NUMBER:
    case CKA_START_DATE:
    case CKA_END_DATE:
    case CKA_DERIVE:
    case CKA_ENCRYPT:
    case CKA_DECRYPT:
    case CKA_SIGN:
    case CKA_VERIFY:
    case CKA_SIGN_RECOVER:
    case CKA_VERIFY_RECOVER:
    case CKA_WRAP:
    case CKA_UNWRAP:
	mtype = PK11_ALWAYS;
	break;

    /* DEPENDS ON CLASS */
    case CKA_VALUE:
	mtype = (inClass == CKO_DATA) ? PK11_ALWAYS : PK11_NEVER;
	break;

    case CKA_SUBJECT:
	mtype = (inClass == CKO_CERTIFICATE) ? PK11_NEVER : PK11_ALWAYS;
	break;
    default:
	break;
    }
    return mtype;
}

/* decode if a particular attribute is sensitive (cannot be read
 * back to the user of if the object is set to SENSITIVE) */
PRBool
fort11_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass) {
    switch(type) {
    /* ALWAYS */
    case CKA_PRIVATE_EXPONENT:
    case CKA_PRIME_1:
    case CKA_PRIME_2:
    case CKA_EXPONENT_1:
    case CKA_EXPONENT_2:
    case CKA_COEFFICIENT:
	return PR_TRUE;

    /* DEPENDS ON CLASS */
    case CKA_VALUE:
	/* PRIVATE and SECRET KEYS have SENSITIVE values */
	return (PRBool)((inClass == CKO_PRIVATE_KEY) || 
			(inClass == CKO_SECRET_KEY));

    default:
	break;
    }
    return PR_FALSE;
}

static void
fort11_DeleteAttributeType(PK11Object *object,CK_ATTRIBUTE_TYPE type) {
    PK11Attribute *attribute;
    attribute = fort11_FindAttribute(object, type);
    if (attribute == NULL) return ;
    fort11_DeleteAttribute(object,attribute);
}


/*
 * create a new nession. NOTE: The session handle is not set, and the
 * session is not added to the slot's session queue.
 */
static PK11Session *
fort11_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify, 
		  CK_VOID_PTR pApplication,
		  CK_FLAGS flags) {
    PK11Session *session;
    PK11Slot *slot = &fort11_slot[slotID-1];
    CK_RV mrv;

    if (slot == NULL) return NULL;

    session = (PK11Session*)PORT_Alloc(sizeof(PK11Session));
    if (session == NULL) return NULL;

    session->next = session->prev = NULL;
    session->refCount = 1;
    session->context = NULL;
    session->search = NULL;
    session->objectIDCount = 1;
    session->fortezzaContext.fortezzaKey = NULL;
    session->fortezzaContext.fortezzaSocket = NULL;

    if (FMUTEX_MutexEnabled()) {
        mrv = FMUTEX_Create(&session->refLock);
	if (mrv != CKR_OK) {
	    PORT_Free(session);
	    return NULL;
	}
	mrv = FMUTEX_Create(&session->objectLock);
	if (mrv != CKR_OK) {
	    FMUTEX_Destroy(session->refLock);
	    PORT_Free(session);
	    return NULL;
	}
    } else {
        session->refLock    = NULL;
	session->objectLock = NULL;
    }

    session->objects[0] = NULL;

    session->slot = slot;
    session->notify = notify;
    session->appData = pApplication;
    session->info.flags = flags;
    session->info.slotID = slotID;
    fort11_update_state(slot,session);
    return session;
}


/*
 * look up a session structure from a session handle
 * generate a reference to it.
 */
PK11Session *
fort11_SessionFromHandle(CK_SESSION_HANDLE handle, PRBool isCloseSession) {
    PK11Slot	*slot = fort11_SlotFromSessionHandle(handle);
    PK11Session *session;

    if (!isCloseSession && 
	!SocketStateUnchanged(&fortezzaSockets[slot->slotID-1]))
        return NULL;

    FMUTEX_Lock(slot->sessionLock);
    pk11queue_find(session,handle,slot->head,SESSION_HASH_SIZE);
    if (session) session->refCount++;
    FMUTEX_Unlock(slot->sessionLock);

    return (session);
}

/* free all the data associated with a session. */
static void
fort11_DestroySession(PK11Session *session)
{
    PK11ObjectList *op,*next;
/*    PORT_Assert(session->refCount == 0);*/

    /* clean out the attributes */
    FMUTEX_Lock(session->objectLock);
    for (op = session->objects[0]; op != NULL; op = next) {
        next = op->next;
        /* paranoia */
	op->next = op->prev = NULL;
	fort11_DeleteObject(session,op->parent);
    }
    FMUTEX_Unlock(session->objectLock);

    FMUTEX_Destroy(session->objectLock);
    FMUTEX_Destroy(session->refLock);

    if (session->search) {
	fort11_FreeSearch(session->search);
    }

    pk11queue_delete(session, session->handle, session->slot->head,
		     SESSION_HASH_SIZE);

    PORT_Free(session);
}


/*
 * release a reference to a session handle
 */
void
fort11_FreeSession(PK11Session *session) {
    PRBool destroy = PR_FALSE;
    PK11Slot *slot = NULL;

    if (!session) return;  /*Quick fix to elminate crash*/
                           /*Fix in later version       */

    if (FMUTEX_MutexEnabled()) {
        slot = fort11_SlotFromSession(session);
	FMUTEX_Lock(slot->sessionLock);
    }
    if (session->refCount == 1) destroy = PR_TRUE;
    session->refCount--;
    if (FMUTEX_MutexEnabled()) {
        FMUTEX_Unlock(slot->sessionLock);
    }

    if (destroy) {
      fort11_DestroySession(session);
    }
}


/* return true if the object matches the template */
PRBool
fort11_objectMatch(PK11Object *object,CK_ATTRIBUTE_PTR theTemplate,int count) {
    int i;

    for (i=0; i < count; i++) {
	PK11Attribute *attribute = 
	    fort11_FindAttribute(object,theTemplate[i].type);
	if (attribute == NULL) {
	    return PR_FALSE;
	}
	if (attribute->attrib.ulValueLen == theTemplate[i].ulValueLen) {
	    if (PORT_Memcmp(attribute->attrib.pValue,theTemplate[i].pValue,
					theTemplate[i].ulValueLen) == 0) {
                fort11_FreeAttribute(attribute);
		continue;
	    }
	}
        fort11_FreeAttribute(attribute);
	return PR_FALSE;
    }
    return PR_TRUE;
}

/* search through all the objects in the queue and return the template matches
 * in the object list.
 */
CK_RV
fort11_searchObjectList(PK11ObjectListElement **objectList,PK11Object **head,
			void *lock, CK_ATTRIBUTE_PTR theTemplate, int count) {
    int i;
    PK11Object *object;
    CK_RV rv;

    for(i=0; i < HASH_SIZE; i++) {
        /* We need to hold the lock to copy a consistant version of
         * the linked list. */
        FMUTEX_Lock(lock);
	for (object = head[i]; object != NULL; object= object->next) {
	    if (fort11_objectMatch(object,theTemplate,count)) {
	        rv = fort11_AddToList(objectList,object);
		if (rv != CKR_OK) {
		    return rv;
		}
	    }
	}
	FMUTEX_Unlock(lock);
    }
    return CKR_OK;
}

static PRBool
fort11_NotAllFuncsNULL (CK_C_INITIALIZE_ARGS_PTR pArgs) {
    return (PRBool)(pArgs && pArgs->CreateMutex && pArgs->DestroyMutex &&
		    pArgs->LockMutex   && pArgs->UnlockMutex);
}

static PRBool
fort11_InArgCheck(CK_C_INITIALIZE_ARGS_PTR pArgs) {
    PRBool rv;
    /* The only check for now, is to make sure that all of the
     * function pointers are either all NULL or all Non-NULL.
     * We also need to make sure the pReserved field in pArgs is
     * set to NULL.
     */
    if (pArgs == NULL) {
        return  PR_TRUE; /* If the argument is NULL, no
			  * inconsistencies can exist.
			  */
    }

    if (pArgs->pReserved != NULL) {
        return PR_FALSE;
    }

    if (pArgs->CreateMutex != NULL) {
        rv = (PRBool) (pArgs->DestroyMutex != NULL &&
		       pArgs->LockMutex    != NULL &&
		       pArgs->UnlockMutex  != NULL);
    } else { /*pArgs->CreateMutex == NULL*/
        rv = (PRBool) (pArgs->DestroyMutex == NULL &&
		       pArgs->LockMutex    == NULL &&
		       pArgs->UnlockMutex  == NULL);
    }
    return rv;
}



/**********************************************************************
 *
 *     Start of PKCS 11 functions 
 *
 **********************************************************************/
 
 
/**********************************************************************
 *
 * In order to get this to work on 68K, we have to do some special tricks,
 * First trick is that we need to make the module a Code Resource, and
 * all Code Resources on 68K have to have a main function.  So we 
 * define main to be a wrapper for C_GetFunctionList which will be the
 * first funnction called by any software that uses the PKCS11 module.
 *
 * The second trick is that whenever you access a global variable from
 * the Code Resource, it does funny things to the stack on 68K, so we 
 * need to call some macros that handle the stack for us.  First thing
 * you do is call EnterCodeResource() first thing in a function that 
 * accesses a global, right before you leave that function, you call 
 * ExitCodeResource.  This will take care of stack management.
 *
 * Third trick is to call __InitCode__() when we first enter the module
 * so that all of the global variables get initialized properly.
 *
 **********************************************************************/ 
 
#if defined(XP_MAC) && !defined(__POWERPC__)

#define FORT11_RETURN(exp)  {ExitCodeResource(); return (exp);}
#define FORT11_ENTER() EnterCodeResource();

#else /*XP_MAC*/

#define FORT11_RETURN(exp)  return (exp);
#define FORT11_ENTER() 

#endif /*XP_MAC*/

#define CARD_OK(rv)   if ((rv) != CI_OK) FORT11_RETURN (CKR_DEVICE_ERROR); 
#define SLOT_OK(slot) if ((slot) > kNumSockets) FORT11_RETURN (CKR_SLOT_ID_INVALID);
 
#ifdef XP_MAC 
/* This is not a 4.0 project, so I can't depend on
 * 4.0 defines, so instead I depend on CodeWarrior 
 * defines.
 */
#if __POWERPC__
#elif __CFM68K__
#else
/* To get this to work on 68K, we need to have
 * the symbol main.  So we just make it a wrapper for C_GetFunctionList.
 */
PR_PUBLIC_API(CK_RV) main(CK_FUNCTION_LIST_PTR *pFunctionList) {
    FORT11_ENTER()
    CK_RV rv;
  
    __InitCode__();
  
    rv = C_GetFunctionList(pFunctionList);
    FORT11_RETURN (rv);
}
#endif

#endif /*XP_MAC*/

/* Return the function list */
PR_PUBLIC_API(CK_RV) C_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList) {
    /* No need to do a FORT11_RETURN as this function will never be directly
     * called in the case where we need to do stack management.  
     * The main function will call this after taking care of stack stuff.
     */
    *pFunctionList = &fort11_funcList;
    return CKR_OK;
}


/* C_Initialize initializes the Cryptoki library. */
PR_PUBLIC_API(CK_RV) C_Initialize(CK_VOID_PTR pReserved) {
    FORT11_ENTER()
    int i,j, tempNumSockets;
    int rv = 1;
    CK_C_INITIALIZE_ARGS_PTR pArgs = (CK_C_INITIALIZE_ARGS_PTR)pReserved;
    CK_RV mrv;

    /* intialize all the slots */
    if (!init) {
      init = PR_TRUE;

      /* need to initialize locks before MACI_Initialize is called in
       * software fortezza. */
      if (pArgs) {
	  if (!fort11_InArgCheck(pArgs)) {
	      FORT11_RETURN (CKR_ARGUMENTS_BAD);
	  }
	  if (pArgs->flags & CKF_OS_LOCKING_OK){
	      if (!fort11_NotAllFuncsNULL(pArgs)) {
		  FORT11_RETURN (CKR_CANT_LOCK);
	      }
	  }
	  if (fort11_NotAllFuncsNULL(pArgs)) {
	      mrv = FMUTEX_Init(pArgs);
	      if (mrv != CKR_OK) {
		  return CKR_GENERAL_ERROR;
	      }
	  }
      }
      rv = MACI_Initialize (&tempNumSockets);
      kNumSockets = (CK_ULONG)tempNumSockets;
      
      CARD_OK (rv);
      for (i=0; i < (int) kNumSockets; i++) {
	if (FMUTEX_MutexEnabled()) {
	    mrv = FMUTEX_Create(&fort11_slot[i].sessionLock);
	    if (mrv != CKR_OK) {
	        FORT11_RETURN (CKR_GENERAL_ERROR);
	    }
	    mrv = FMUTEX_Create(&fort11_slot[i].objectLock);
	    if (mrv != CKR_OK) {
	        FMUTEX_Destroy(fort11_slot[i].sessionLock);
	        FORT11_RETURN (CKR_GENERAL_ERROR);
	    }
	} else {
	    fort11_slot[i].sessionLock = NULL;
	    fort11_slot[i].objectLock  = NULL;
	}
	for(j=0; j < SESSION_HASH_SIZE; j++) {
	  fort11_slot[i].head[j] = NULL;
	}
	for(j=0; j < HASH_SIZE; j++) {
	  fort11_slot[i].tokObjects[j] = NULL;
	}
	fort11_slot[i].password = NULL;
	fort11_slot[i].hasTokens = PR_FALSE;
	fort11_slot[i].sessionIDCount = fort11_firstSessionID (i);
	fort11_slot[i].sessionCount = 0;
	fort11_slot[i].rwSessionCount = 0;
	fort11_slot[i].tokenIDCount = 1;
	fort11_slot[i].needLogin = PR_TRUE;
	fort11_slot[i].isLoggedIn = PR_FALSE;
	fort11_slot[i].ssoLoggedIn = PR_FALSE;
	fort11_slot[i].DB_loaded = PR_FALSE;
	fort11_slot[i].slotID= i+1;
	InitSocket(&fortezzaSockets[i], i+1);
      }
    }
    FORT11_RETURN (CKR_OK);
}

/*C_Finalize indicates that an application is done with the Cryptoki library.*/
PR_PUBLIC_API(CK_RV) C_Finalize (CK_VOID_PTR pReserved) {
    FORT11_ENTER()
    int i;

    for (i=0; i< (int) kNumSockets; i++) {
        FreeSocket(&fortezzaSockets[i]);
    }
    MACI_Terminate(fortezzaSockets[0].maciSession);
    init = PR_FALSE;
    FORT11_RETURN (CKR_OK);
}




/* C_GetInfo returns general information about Cryptoki. */
PR_PUBLIC_API(CK_RV)  C_GetInfo(CK_INFO_PTR pInfo) {
    FORT11_ENTER()
    pInfo->cryptokiVersion = fort11_funcList.version;
    PORT_Memcpy(pInfo->manufacturerID,manufacturerID,32);
    pInfo->libraryVersion.major = 1;
    pInfo->libraryVersion.minor = 7;
    PORT_Memcpy(pInfo->libraryDescription,libraryDescription,32);
    pInfo->flags = 0;
    FORT11_RETURN (CKR_OK);
}

/* C_GetSlotList obtains a list of slots in the system. */
PR_PUBLIC_API(CK_RV) C_GetSlotList(CK_BBOOL	   tokenPresent,
				   CK_SLOT_ID_PTR pSlotList, 
				   CK_ULONG_PTR   pulCount) {
    FORT11_ENTER()
    int i;

    if (pSlotList != NULL) {
      if (*pulCount >= kNumSockets) {
	for (i=0; i < (int) kNumSockets; i++) {
	  pSlotList[i] = i+1; 
	}
      } else {
	FORT11_RETURN (CKR_BUFFER_TOO_SMALL); 
      }
    } else {
      *pulCount = kNumSockets;
    }
    FORT11_RETURN (CKR_OK);
}
	
/* C_GetSlotInfo obtains information about a particular slot in the system. */
PR_PUBLIC_API(CK_RV) C_GetSlotInfo(CK_SLOT_ID       slotID, 
				   CK_SLOT_INFO_PTR pInfo) {
  FORT11_ENTER()
    int        rv;
    CI_CONFIG  ciConfig;
    CI_STATE   ciState;
    HSESSION   maciSession;
    char       slotDescription[65];
    FortezzaSocket *socket;
    

    SLOT_OK(slotID);
    
    socket = &fortezzaSockets[slotID-1];
    if (!socket->isOpen) {
        InitSocket(socket, slotID);
    }
    maciSession = socket->maciSession;

    rv = MACI_Select(maciSession, slotID);

    CARD_OK (rv)

    rv = MACI_GetConfiguration (maciSession, &ciConfig);


    pInfo->firmwareVersion.major = 0;
    pInfo->firmwareVersion.minor = 0;
#ifdef SWFORT
    PORT_Memcpy (pInfo->manufacturerID,"Netscape Communications Corp    ",32);
    PORT_Memcpy (slotDescription,"Netscape Software Slot #        ",32);
#define _local_BASE 24
#else
    PORT_Memcpy (pInfo->manufacturerID,"LITRONIC                        ",32);
    PORT_Memcpy (slotDescription,"Litronic MACI Slot #            ",32);
#define _local_BASE 20
#endif
    slotDescription[_local_BASE] = (char )((slotID < 10) ? slotID : 
							slotID/10) + '0';
    if (slotID >= 10) slotDescription[_local_BASE+1] = 
						(char)(slotID % 10) + '0';
    PORT_Memcpy (&slotDescription[32],"                                ",32);
    PORT_Memcpy (pInfo->slotDescription, slotDescription          , 64);
    if (rv == CI_OK) {
        pInfo->hardwareVersion.major = 
	    (ciConfig.ManufacturerVersion & MAJOR_VERSION_MASK) >> 8;
	pInfo->hardwareVersion.minor = 
	    ciConfig.ManufacturerVersion & MINOR_VERSION_MASK;
	pInfo->flags = CKF_TOKEN_PRESENT;
    } else {
       pInfo->hardwareVersion.major = 0;
       pInfo->hardwareVersion.minor = 0;
       pInfo->flags = 0;
    }
#ifdef SWFORT
    /* do we need to make it a removable device as well?? */
    pInfo->flags |= CKF_REMOVABLE_DEVICE;
#else
    pInfo->flags |= (CKF_REMOVABLE_DEVICE | CKF_HW_SLOT);
#endif
    
    rv = MACI_GetState(maciSession, &ciState);
 
    if (rv == CI_OK) {
        switch (ciState) {
	case CI_ZEROIZE:
	case CI_INTERNAL_FAILURE:
	    pInfo->flags &= (~CKF_TOKEN_PRESENT);
	default:
	    break;
	}
    } else {
        pInfo->flags &= (~CKF_TOKEN_PRESENT);
    }

    FORT11_RETURN (CKR_OK);
}

#define CKF_THREAD_SAFE 0x8000 

/* C_GetTokenInfo obtains information about a particular token
   in the system. */
PR_PUBLIC_API(CK_RV) C_GetTokenInfo(CK_SLOT_ID        slotID,
				    CK_TOKEN_INFO_PTR pInfo) {
	FORT11_ENTER()
    CI_STATUS cardStatus;
    CI_CONFIG ciConfig;
    PK11Slot *slot;
    int rv, i;
    char tmp[33];
    FortezzaSocket *socket;

    SLOT_OK (slotID);
    
    slot = &fort11_slot[slotID-1];
    
    socket = &fortezzaSockets[slotID-1];
    if (!socket->isOpen) {
        InitSocket(socket, slotID);
    }

    rv = MACI_Select (socket->maciSession, slotID);
    rv = MACI_GetStatus (socket->maciSession, &cardStatus);
    if (rv != CI_OK) {
        FORT11_RETURN (CKR_DEVICE_ERROR);
    }

#ifdef SWFORT
    sprintf (tmp, "Software FORTEZZA Slot #%d", slotID);
#else
    sprintf (tmp, "FORTEZZA Slot #%d", slotID);
#endif
    
    PORT_Memcpy (pInfo->label, tmp, PORT_Strlen(tmp)+1);

    for (i=0; i<8; i++) {
        int serNum;

	serNum = (int)cardStatus.SerialNumber[i];
	sprintf ((char*)&pInfo->serialNumber[2*i], "%.2x", serNum);
    }

    rv = MACI_GetTime (fortezzaSockets[slotID-1].maciSession, pInfo->utcTime);
    if (rv == CI_OK) {
      pInfo->flags = CKF_CLOCK_ON_TOKEN;  
    } else {
      switch (rv) {
      case CI_LIB_NOT_INIT:
      case CI_INV_POINTER:
      case CI_NO_CARD:
      case CI_NO_SOCKET:
	FORT11_RETURN (CKR_DEVICE_ERROR);
      default:
	pInfo->flags = 0;
	break;
      }
    }
    
    rv = MACI_GetConfiguration (fortezzaSockets[slotID-1].maciSession, 
				&ciConfig);

    if (rv == CI_OK) {
        PORT_Memcpy(pInfo->manufacturerID,ciConfig.ManufacturerName,
		    PORT_Strlen(ciConfig.ManufacturerName));
	for (i=PORT_Strlen(ciConfig.ManufacturerName); i<32; i++) {
	    pInfo->manufacturerID[i] = ' ';
	}
	PORT_Memcpy(pInfo->model,ciConfig.ProcessorType,16);    
    }
    pInfo->ulMaxPinLen = CI_PIN_SIZE;
    pInfo->ulMinPinLen = 0;
    pInfo->ulTotalPublicMemory = 0;
    pInfo->ulFreePublicMemory  = 0;
    pInfo->flags |= CKF_RNG | CKF_LOGIN_REQUIRED| CKF_USER_PIN_INITIALIZED | 
                    CKF_THREAD_SAFE | CKF_WRITE_PROTECTED;

    pInfo->ulMaxSessionCount = 0; 
    pInfo->ulSessionCount = slot->sessionCount; 
    pInfo->ulMaxRwSessionCount = 0; 
    pInfo->ulRwSessionCount = slot->rwSessionCount; 

    if (rv == CI_OK) {    
        pInfo->firmwareVersion.major = 
	    (ciConfig.ManufacturerSWVer & MAJOR_VERSION_MASK) >> 8; 
	pInfo->firmwareVersion.minor = 
	    ciConfig.ManufacturerSWVer & MINOR_VERSION_MASK;
	pInfo->hardwareVersion.major = 
	    (ciConfig.ManufacturerVersion & MAJOR_VERSION_MASK) >> 8;
	pInfo->hardwareVersion.minor = 
	    ciConfig.ManufacturerVersion & MINOR_VERSION_MASK;
    }
    FORT11_RETURN (CKR_OK);
}



/* C_GetMechanismList obtains a list of mechanism types supported by a 
   token. */
PR_PUBLIC_API(CK_RV) C_GetMechanismList(CK_SLOT_ID            slotID,
					CK_MECHANISM_TYPE_PTR pMechanismList, 
					CK_ULONG_PTR          pulCount) {
  FORT11_ENTER()
  CK_RV rv = CKR_OK;
  int i;
  
  SLOT_OK (slotID);

  if (pMechanismList == NULL) {
    *pulCount = mechanismCount;
  } else {
    if (*pulCount >= mechanismCount) {
      *pulCount = mechanismCount;
      for (i=0; i< (int)mechanismCount; i++) {
	pMechanismList[i] = mechanisms[i].type;
      }
    } else {
      rv = CKR_BUFFER_TOO_SMALL;
    }
  }
  FORT11_RETURN (rv);
}


/* C_GetMechanismInfo obtains information about a particular mechanism 
 * possibly supported by a token. */
PR_PUBLIC_API(CK_RV) C_GetMechanismInfo(CK_SLOT_ID            slotID, 
					CK_MECHANISM_TYPE     type,
					CK_MECHANISM_INFO_PTR pInfo) {
  int i;
  FORT11_ENTER()
  SLOT_OK (slotID);

  for (i=0; i< (int)mechanismCount; i++) {
    if (type == mechanisms[i].type) {
      PORT_Memcpy (pInfo, &mechanisms[i].domestic, sizeof (CK_MECHANISM_INFO));
      FORT11_RETURN (CKR_OK);
    }
  }
  FORT11_RETURN (CKR_MECHANISM_INVALID);
}


/* C_InitToken initializes a token. */
PR_PUBLIC_API(CK_RV) C_InitToken(CK_SLOT_ID  slotID,
				 CK_CHAR_PTR pPin,
				 CK_ULONG    ulPinLen,
				 CK_CHAR_PTR pLabel) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_InitPIN initializes the normal user's PIN. */
PR_PUBLIC_API(CK_RV) C_InitPIN(CK_SESSION_HANDLE hSession,
			       CK_CHAR_PTR       pPin, 
			       CK_ULONG          ulPinLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_SetPIN modifies the PIN of user that is currently logged in. */
/* NOTE: This is only valid for the PRIVATE_KEY_SLOT */
PR_PUBLIC_API(CK_RV) C_SetPIN(CK_SESSION_HANDLE hSession, 
			      CK_CHAR_PTR       pOldPin,
			      CK_ULONG          ulOldLen, 
			      CK_CHAR_PTR       pNewPin, 
			      CK_ULONG          ulNewLen) {
  FORT11_ENTER()
#ifndef SWFORT
  CI_PIN       ciOldPin, ciNewPin;
#endif
  PK11Session *session;
  PK11Slot    *slot;
  int          rv;

  session = fort11_SessionFromHandle (hSession, PR_FALSE);

  slot = fort11_SlotFromSession (session);
  SLOT_OK(slot->slotID)

  if (session == NULL) {
      session = fort11_SessionFromHandle (hSession, PR_TRUE);
      fort11_TokenRemoved(slot, session);
      FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
  }

  rv = MACI_Select (fortezzaSockets[slot->slotID-1].maciSession, slot->slotID);
  CARD_OK (rv)
  
  if (slot->needLogin && session->info.state != CKS_RW_USER_FUNCTIONS) {
    fort11_FreeSession (session);
    FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
  }

  fort11_FreeSession (session);

  if (ulNewLen > CI_PIN_SIZE || ulOldLen > CI_PIN_SIZE) 
    FORT11_RETURN (CKR_PIN_LEN_RANGE);

#ifndef SWFORT
  fort11_convertToCIPin (ciOldPin,pOldPin, ulOldLen);
  fort11_convertToCIPin (ciNewPin,pNewPin, ulNewLen);

  rv = MACI_ChangePIN (fortezzaSockets[slot->slotID-1].maciSession, 
		       CI_USER_PIN, ciOldPin, ciNewPin);
#else
  rv = MACI_ChangePIN (fortezzaSockets[slot->slotID-1].maciSession, 
		       CI_USER_PIN,  pOldPin, pNewPin);
#endif

  if (rv != CI_OK) {
    switch (rv) {
    case CI_FAIL:
      FORT11_RETURN (CKR_PIN_INCORRECT);
    default:
      FORT11_RETURN (CKR_DEVICE_ERROR);
    }
  } 
  FORT11_RETURN (CKR_OK);
}

/* C_OpenSession opens a session between an application and a token. */
PR_PUBLIC_API(CK_RV) C_OpenSession(CK_SLOT_ID            slotID, 
				   CK_FLAGS              flags,
				   CK_VOID_PTR           pApplication,
				   CK_NOTIFY             Notify,
				   CK_SESSION_HANDLE_PTR phSession) {
  FORT11_ENTER()
  PK11Slot          *slot;
  CK_SESSION_HANDLE  sessionID;
  PK11Session       *session;
  FortezzaSocket    *socket;

  SLOT_OK (slotID)
  slot = &fort11_slot[slotID-1];
  socket = &fortezzaSockets[slotID-1];

  if (!socket->isOpen) {
      if (InitSocket(socket, slotID) != SOCKET_SUCCESS) {
	  FORT11_RETURN (CKR_TOKEN_NOT_PRESENT);
      }
  }

  session = fort11_NewSession (slotID, Notify, pApplication, 
			       flags | CKF_SERIAL_SESSION);

  if (session == NULL) FORT11_RETURN (CKR_HOST_MEMORY);

  FMUTEX_Lock(slot->sessionLock);

  slot->sessionIDCount += ADD_NEXT_SESS_ID;
  sessionID = slot->sessionIDCount;
  fort11_update_state (slot, session);
  pk11queue_add (session, sessionID, slot->head, SESSION_HASH_SIZE);
  slot->sessionCount++;
  if (session->info.flags & CKF_RW_SESSION) {
    slot->rwSessionCount++;
  }
  session->handle = sessionID;
  session->info.ulDeviceError = 0;

  FMUTEX_Unlock(slot->sessionLock);
  
  *phSession = sessionID;
  FORT11_RETURN (CKR_OK);
}


/* C_CloseSession closes a session between an application and a token. */
PR_PUBLIC_API(CK_RV) C_CloseSession(CK_SESSION_HANDLE hSession) {
  FORT11_ENTER()
  PK11Slot    *slot;
  PK11Session *session;
  
  session = fort11_SessionFromHandle  (hSession, PR_TRUE);
  slot = fort11_SlotFromSessionHandle (hSession);

  if (session == NULL) {
      FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
  }
  
  FMUTEX_Lock(slot->sessionLock);
  if (session->next || session->prev) {
    session->refCount--;
    if (session->info.flags & CKF_RW_SESSION) {
      slot->rwSessionCount--;
    }
    if (slot->sessionCount == 0) {
      slot->isLoggedIn = PR_FALSE;
      slot->password = NULL;
    }
  }

  FMUTEX_Unlock(slot->sessionLock);
  
  fort11_FreeSession (session);
  FORT11_RETURN (CKR_OK);
}


/* C_CloseAllSessions closes all sessions with a token. */
PR_PUBLIC_API(CK_RV) C_CloseAllSessions (CK_SLOT_ID slotID) {
  FORT11_ENTER()
  PK11Slot *slot;
  PK11Session *session;
  int i;
  
  
  slot = fort11_SlotFromID(slotID);
  if (slot == NULL) FORT11_RETURN (CKR_SLOT_ID_INVALID);
  
  /* first log out the card */
  FMUTEX_Lock(slot->sessionLock);
  slot->isLoggedIn = PR_FALSE;
  slot->password = NULL;
  FMUTEX_Unlock(slot->sessionLock);
  
  /* now close all the current sessions */
  /* NOTE: If you try to open new sessions before C_CloseAllSessions
   * completes, some of those new sessions may or may not be closed by
   * C_CloseAllSessions... but any session running when this code starts
   * will guarrenteed be close, and no session will be partially closed */
  for (i=0; i < SESSION_HASH_SIZE; i++) {
    do {
      FMUTEX_Lock(slot->sessionLock);
      session = slot->head[i];
      /* hand deque */
      /* this duplicates much of C_close session functionality, but because
       * we know that we are freeing all the sessions, we and do some
       * more efficient processing */
      if (session) {
	slot->head[i] = session->next;
	if (session->next) session->next->prev = NULL;
	session->next = session->prev = NULL;
	slot->sessionCount--;
	if (session->info.flags & CKF_RW_SESSION) {
	  slot->rwSessionCount--;
	}
      }
      FMUTEX_Unlock(slot->sessionLock);
      if (session) fort11_FreeSession(session);
    } while (session != NULL);
  }
  FORT11_RETURN (CKR_OK);
}


/* C_GetSessionInfo obtains information about the session. */
PR_PUBLIC_API(CK_RV) C_GetSessionInfo(CK_SESSION_HANDLE   hSession,
				      CK_SESSION_INFO_PTR pInfo) {
    FORT11_ENTER()
    PK11Session    *session;
    PK11Slot       *slot;
    CI_STATE        cardState;
    FortezzaSocket *socket;
    int             ciRV;
    
    session = fort11_SessionFromHandle (hSession, PR_FALSE);
    slot    = fort11_SlotFromSessionHandle(hSession);
    socket  = &fortezzaSockets[slot->slotID-1];
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    PORT_Memcpy (pInfo, &session->info, sizeof (CK_SESSION_INFO));
    fort11_FreeSession(session);

    ciRV = MACI_Select(socket->maciSession, slot->slotID);
    CARD_OK(ciRV)

    ciRV = MACI_GetState(socket->maciSession, &cardState);
    CARD_OK(ciRV)

    if (socket->isLoggedIn) { 
        switch (cardState) {
	case CI_POWER_UP:
	case CI_UNINITIALIZED:
	case CI_INITIALIZED:
	case CI_SSO_INITIALIZED:
	case CI_LAW_INITIALIZED:
	case CI_USER_INITIALIZED:
	  pInfo->state = CKS_RO_PUBLIC_SESSION;
	  break;
	case CI_STANDBY:
	case CI_READY:
	  pInfo->state = CKS_RO_USER_FUNCTIONS;
	  break;
	default:
	  pInfo->state = CKS_RO_PUBLIC_SESSION;
	  break;
	}
    } else {
        pInfo->state = CKS_RO_PUBLIC_SESSION;
    }

    FORT11_RETURN (CKR_OK);
}

/* C_Login logs a user into a token. */
PR_PUBLIC_API(CK_RV) C_Login(CK_SESSION_HANDLE hSession,
			     CK_USER_TYPE      userType,
			     CK_CHAR_PTR       pPin, 
			     CK_ULONG          ulPinLen) {
  FORT11_ENTER()
  PK11Slot    *slot;
  PK11Session *session;
#ifndef SWFORT
  CI_PIN       ciPin;
#endif
  int          rv, ciUserType;

  slot = fort11_SlotFromSessionHandle (hSession);
  session = fort11_SessionFromHandle(hSession, PR_FALSE);

  if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
  }

  fort11_FreeSession(session);

  if (slot->isLoggedIn) FORT11_RETURN (CKR_USER_ALREADY_LOGGED_IN);
  slot->ssoLoggedIn = PR_FALSE;

#ifndef SWFORT
  if (ulPinLen > CI_PIN_SIZE) FORT11_RETURN (CKR_PIN_LEN_RANGE);

  fort11_convertToCIPin (ciPin, pPin, ulPinLen);
#endif
  switch (userType) {
  case CKU_SO:
    ciUserType = CI_SSO_PIN;
    break;
  case CKU_USER:
    ciUserType = CI_USER_PIN;
    break;
  default:
    FORT11_RETURN (CKR_USER_TYPE_INVALID);
  }
  
#ifndef SWFORT
  rv = LoginToSocket(&fortezzaSockets[slot->slotID-1], ciUserType, ciPin);
#else
  rv = LoginToSocket(&fortezzaSockets[slot->slotID-1], ciUserType, pPin);
#endif

  switch (rv) {
  case SOCKET_SUCCESS:
    break;
  case CI_FAIL:
    FORT11_RETURN (CKR_PIN_INCORRECT);
  default:
    FORT11_RETURN (CKR_DEVICE_ERROR);
  }

  FMUTEX_Lock(slot->sessionLock);
  slot->isLoggedIn = PR_TRUE;
  if (userType == CKU_SO) {
    slot->ssoLoggedIn = PR_TRUE;
  }
  FMUTEX_Unlock(slot->sessionLock);
  
  fort11_update_all_states(slot);
  FORT11_RETURN (CKR_OK);
}

/* C_Logout logs a user out from a token. */
PR_PUBLIC_API(CK_RV) C_Logout(CK_SESSION_HANDLE hSession) {
    FORT11_ENTER()
    PK11Slot    *slot    = fort11_SlotFromSessionHandle(hSession);
    PK11Session *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (!slot->isLoggedIn)
      FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
    
    FMUTEX_Lock(slot->sessionLock);

    slot->isLoggedIn  = PR_FALSE;
    slot->ssoLoggedIn = PR_FALSE;
    slot->password    = NULL;
    LogoutFromSocket  (&fortezzaSockets[slot->slotID-1]);

    FMUTEX_Unlock(slot->sessionLock);
      
    fort11_update_all_states(slot);
    FORT11_RETURN (CKR_OK);
}

/* C_CreateObject creates a new object. */
PR_PUBLIC_API(CK_RV) C_CreateObject(CK_SESSION_HANDLE    hSession,
				    CK_ATTRIBUTE_PTR     pTemplate, 
				    CK_ULONG             ulCount, 
				    CK_OBJECT_HANDLE_PTR phObject) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_CopyObject copies an object, creating a new object for the copy. */
PR_PUBLIC_API(CK_RV) C_CopyObject(CK_SESSION_HANDLE    hSession,
				  CK_OBJECT_HANDLE     hObject, 
				  CK_ATTRIBUTE_PTR     pTemplate, 
				  CK_ULONG             ulCount,
				  CK_OBJECT_HANDLE_PTR phNewObject) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_DestroyObject destroys an object. */
PR_PUBLIC_API(CK_RV) C_DestroyObject(CK_SESSION_HANDLE hSession,
				     CK_OBJECT_HANDLE  hObject) {
    FORT11_ENTER()
    PK11Slot *slot = fort11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Object *object;
    PK11FreeStatus status;

    /*
     * This whole block just makes sure we really can destroy the
     * requested object.
     */
    session = fort11_SessionFromHandle(hSession, PR_FALSE);
    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    object = fort11_ObjectFromHandle(hObject,session);
    if (object == NULL) {
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OBJECT_HANDLE_INVALID);
    }

    /* don't destroy a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(fort11_isTrue(object,CKA_PRIVATE))) {
	fort11_FreeSession(session);
	fort11_FreeObject(object);
	FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
    }

    /* don't destroy a token object if we aren't in a rw session */

    if (((session->info.flags & CKF_RW_SESSION) == 0) &&
				(fort11_isTrue(object,CKA_TOKEN))) {
	fort11_FreeSession(session);
	fort11_FreeObject(object);
	FORT11_RETURN (CKR_SESSION_READ_ONLY);
    }

    /* ACTUALLY WE NEED TO DEAL WITH TOKEN OBJECTS AS WELL */
    FMUTEX_Lock(session->objectLock);
    fort11_DeleteObject(session,object);
    FMUTEX_Unlock(session->objectLock);

    fort11_FreeSession(session);

    /*
     * get some indication if the object is destroyed. Note: this is not
     * 100%. Someone may have an object reference outstanding (though that
     * should not be the case by here. Also now that the object is "half"
     * destroyed. Our internal representation is destroyed, but it is still
     * in the data base.
     */
    status = fort11_FreeObject(object);

    FORT11_RETURN ((status != PK11_DestroyFailure) ? CKR_OK : CKR_DEVICE_ERROR);
}


/* C_GetObjectSize gets the size of an object in bytes. */
PR_PUBLIC_API(CK_RV) C_GetObjectSize(CK_SESSION_HANDLE hSession,
				     CK_OBJECT_HANDLE  hObject, 
				     CK_ULONG_PTR      pulSize) {
	/* For functions that don't access globals, we don't have to worry about the
     * stack.
     */	
    *pulSize = 0;
    return CKR_OK;
}


/* C_GetAttributeValue obtains the value of one or more object attributes. */
PR_PUBLIC_API(CK_RV) C_GetAttributeValue(CK_SESSION_HANDLE hSession,
					 CK_OBJECT_HANDLE  hObject,
					 CK_ATTRIBUTE_PTR  pTemplate,
					 CK_ULONG          ulCount) {
    FORT11_ENTER()
    PK11Slot *slot = fort11_SlotFromSessionHandle(hSession);
    PK11Session *session;
    PK11Object *object;
    PK11Attribute *attribute;
    PRBool sensitive;
    int i;

    /*
     * make sure we're allowed
     */
    session = fort11_SessionFromHandle(hSession, PR_FALSE);
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    object = fort11_ObjectFromHandle(hObject,session);
    fort11_FreeSession(session);
    if (object == NULL) {
	FORT11_RETURN (CKR_OBJECT_HANDLE_INVALID);
    }

    /* don't read a private object if we aren't logged in */
    if ((!slot->isLoggedIn) && (slot->needLogin) &&
				(fort11_isTrue(object,CKA_PRIVATE))) {
	fort11_FreeObject(object);
	FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
    }

    sensitive = fort11_isTrue(object,CKA_SENSITIVE);
    for (i=0; i <  (int)ulCount; i++) {
	/* Make sure that this attribute is retrievable */
	if (sensitive && fort11_isSensitive(pTemplate[i].type,object->objclass)) {
	    fort11_FreeObject(object);
	    FORT11_RETURN (CKR_ATTRIBUTE_SENSITIVE);
	}
	attribute = fort11_FindAttribute(object,pTemplate[i].type);
	if (attribute == NULL) {
	    fort11_FreeObject(object);
	    FORT11_RETURN (CKR_ATTRIBUTE_TYPE_INVALID);
	}
	if (pTemplate[i].pValue != NULL) {
	    PORT_Memcpy(pTemplate[i].pValue,attribute->attrib.pValue,
			attribute->attrib.ulValueLen);
	}
	pTemplate[i].ulValueLen = attribute->attrib.ulValueLen;
	fort11_FreeAttribute(attribute);
    }

    fort11_FreeObject(object);
    FORT11_RETURN (CKR_OK);
}

/* C_SetAttributeValue modifies the value of one or more object attributes */
PR_PUBLIC_API(CK_RV) C_SetAttributeValue (CK_SESSION_HANDLE hSession,
					  CK_OBJECT_HANDLE  hObject,
					  CK_ATTRIBUTE_PTR  pTemplate,
					  CK_ULONG          ulCount) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/* C_FindObjectsInit initializes a search for token and session objects 
 * that match a template. */
PR_PUBLIC_API(CK_RV) C_FindObjectsInit(CK_SESSION_HANDLE hSession,
				       CK_ATTRIBUTE_PTR  pTemplate,
				       CK_ULONG          ulCount) {
    FORT11_ENTER()
    PK11Slot              *slot = fort11_SlotFromSessionHandle(hSession);
    PK11Session           *session;
    PK11ObjectListElement *objectList = NULL;
    PK11ObjectListElement *olp;
    PK11SearchResults     *search, *freeSearch;
    FortezzaSocket        *currSocket;
    int                    rv, count, i;

    if (slot == NULL) {
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    
    if ((!slot->isLoggedIn) && (slot->needLogin)) 
        FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);

    session = fort11_SessionFromHandle(hSession, PR_FALSE);
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    currSocket = &fortezzaSockets[slot->slotID-1];
    if (currSocket->personalityList == NULL) {
        rv = FetchPersonalityList(currSocket);
	if (rv != SOCKET_SUCCESS) {
	    fort11_FreeSession(session);
	    FORT11_RETURN (CKR_DEVICE_ERROR);
	}

	rv = fort11_BuildCertObjects(currSocket, slot, session);

	if (rv != CKR_OK) {
	    fort11_FreeSession(session);
	    FORT11_RETURN (rv);
	}

	
    }
    rv = fort11_searchObjectList(&objectList, slot->tokObjects, 
				slot->objectLock, pTemplate, ulCount);
    if (rv != CKR_OK) {
      fort11_FreeObjectList(objectList);
      fort11_FreeSession(session);
      FORT11_RETURN (rv);
    }

    /*copy list to session*/

    count = 0;
    for(olp = objectList; olp != NULL; olp = olp->next) {
      count++;
    } 

    search = (PK11SearchResults *)PORT_Alloc(sizeof(PK11SearchResults));
    if (search != NULL) {
	search->handles = (CK_OBJECT_HANDLE *)
				PORT_Alloc(sizeof(CK_OBJECT_HANDLE) * count);
	if (search->handles != NULL) {
	    for (i=0; i < count; i++) {
		search->handles[i] = objectList->object->handle;
		objectList = fort11_FreeObjectListElement(objectList);
	    }
	} else {
	    PORT_Free(search);
	    search = NULL;
	}
    }
    if (search == NULL) {
	fort11_FreeObjectList(objectList);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    /* store the search info */
    search->index = 0;
    search->size = count;
    if ((freeSearch = session->search) != NULL) {
	session->search = NULL;
	fort11_FreeSearch(freeSearch);
    }
    session->search = search;
    fort11_FreeSession(session);
    FORT11_RETURN (CKR_OK);
}


/* C_FindObjects continues a search for token and session objects 
 * that match a template, obtaining additional object handles. */
PR_PUBLIC_API(CK_RV) C_FindObjects(CK_SESSION_HANDLE    hSession,
				   CK_OBJECT_HANDLE_PTR phObject,
				   CK_ULONG             ulMaxObjectCount,
				   CK_ULONG_PTR         pulObjectCount) {
    FORT11_ENTER()
    PK11Session       *session;
    PK11SearchResults *search;
    PK11Slot          *slot;
    int	transfer;
    unsigned long left;

    *pulObjectCount = 0;
    session = fort11_SessionFromHandle(hSession,PR_FALSE);
    slot    = fort11_SlotFromSessionHandle(hSession);
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    if (session->search == NULL) {
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }
    search = session->search;
    left = session->search->size - session->search->index;
    transfer = (ulMaxObjectCount > left) ? left : ulMaxObjectCount;
    PORT_Memcpy(phObject,&search->handles[search->index],
					transfer*sizeof(CK_OBJECT_HANDLE_PTR));
    search->index += transfer;
    if (search->index == search->size) {
	session->search = NULL;
	fort11_FreeSearch(search);
    }
    fort11_FreeSession(session);
    *pulObjectCount = transfer;
    FORT11_RETURN (CKR_OK);
}

/* C_FindObjectsFinal finishes a search for token and session objects. */
PR_PUBLIC_API(CK_RV) C_FindObjectsFinal(CK_SESSION_HANDLE hSession) {
    FORT11_ENTER()
    PK11Session       *session;
    PK11SearchResults *search;
    PK11Slot          *slot;

    session = fort11_SessionFromHandle(hSession, PR_FALSE);
    slot    = fort11_SlotFromSessionHandle(hSession);
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    search = session->search;
    session->search = NULL;
    if (search == NULL) {
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }
    fort11_FreeSearch(search);

    /* UnloadPersonalityList(&fortezzaSockets[session->slot->slotID-1]); */
    fort11_FreeSession(session);
    FORT11_RETURN (CKR_OK);
}


/* C_EncryptInit initializes an encryption operation. */
PR_PUBLIC_API(CK_RV) C_EncryptInit(CK_SESSION_HANDLE hSession,
				   CK_MECHANISM_PTR  pMechanism, 
				   CK_OBJECT_HANDLE  hKey) {
    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    PK11Object      *keyObject;
    FortezzaSocket  *socket    = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    HSESSION         hs        = socket->maciSession;
    FortezzaKey     *fortezzaKey;
    CI_IV            fortezzaIV;
    int              ciRV, registerIndex;
    

    if (pMechanism->mechanism != CKM_SKIPJACK_CBC64) {
        if (session) {
	    fort11_FreeSession(session);
	}
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    
    keyObject = fort11_ObjectFromHandle (hKey, session);

    if (keyObject == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    ciRV = MACI_Select (hs, slot->slotID);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    ciRV = MACI_SetMode(hs, CI_ENCRYPT_TYPE, CI_CBC64_MODE);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    /*Load the correct key into a key register*/
    fortezzaKey = (FortezzaKey*)keyObject->objectInfo;
    fort11_FreeObject (keyObject);
    if (fortezzaKey == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    if (fortezzaKey->keyRegister == KeyNotLoaded) {
        registerIndex = LoadKeyIntoRegister (fortezzaKey);
    } else {
        registerIndex = fortezzaKey->keyRegister;
    }

    if (registerIndex == KeyNotLoaded) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    ciRV = MACI_SetKey (hs,registerIndex);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    ciRV = MACI_GenerateIV(hs, fortezzaIV);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }
    context = &session->fortezzaContext;
    InitContext(context, socket, hKey);
    ciRV = SaveState(context, fortezzaIV, session, fortezzaKey, 
		     CI_ENCRYPT_EXT_TYPE, pMechanism->mechanism);
    if (ciRV != SOCKET_SUCCESS) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    if (pMechanism->pParameter != NULL && 
	pMechanism->ulParameterLen >= sizeof(CI_IV)) {
        PORT_Memcpy (pMechanism->pParameter, fortezzaIV, sizeof(CI_IV));
    }

    InitCryptoOperation(context, Encrypt);
    fort11_FreeSession(session);

    FORT11_RETURN (CKR_OK);
}

/* C_Encrypt encrypts single-part data. */
PR_PUBLIC_API(CK_RV) C_Encrypt (CK_SESSION_HANDLE hSession, 
				CK_BYTE_PTR       pData,
				CK_ULONG          ulDataLen, 
				CK_BYTE_PTR       pEncryptedData,
				CK_ULONG_PTR      pulEncryptedDataLen) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle (hSession, PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket  *socket  = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    HSESSION         hs;
    CK_RV            rv;

    
    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession , PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;
    if (GetCryptoOperation(context) != Encrypt) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_OPERATION_NOT_INITIALIZED);
    }

    *pulEncryptedDataLen = ulDataLen;
    if (pEncryptedData == NULL) {
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    hs = socket->maciSession;
    FMUTEX_Lock(socket->registersLock);
    MACI_Lock(hs, CI_BLOCK_LOCK_FLAG);
    rv = EncryptData (context, pData, ulDataLen, 
		      pEncryptedData, *pulEncryptedDataLen);
    MACI_Unlock(hs);
    FMUTEX_Unlock(socket->registersLock);

    if (rv != SOCKET_SUCCESS) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    EndCryptoOperation(context, Encrypt);
    fort11_FreeSession(session);	      

    FORT11_RETURN (CKR_OK);
}


/* C_EncryptUpdate continues a multiple-part encryption operation. */
PR_PUBLIC_API(CK_RV) C_EncryptUpdate(CK_SESSION_HANDLE hSession,
				     CK_BYTE_PTR       pPart, 
				     CK_ULONG          ulPartLen, 
				     CK_BYTE_PTR       pEncryptedPart,	
				     CK_ULONG_PTR      pulEncryptedPartLen) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle(hSession,PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket  *socket  = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    int              rv;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;
    
    if (GetCryptoOperation(context) != Encrypt) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_OPERATION_NOT_INITIALIZED);
    }
    
    if (pEncryptedPart == NULL) {
        *pulEncryptedPartLen = ulPartLen;
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    if (*pulEncryptedPartLen < ulPartLen) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_BUFFER_TOO_SMALL);
    }

    *pulEncryptedPartLen = ulPartLen;
    
    FMUTEX_Lock(socket->registersLock);
    MACI_Lock(socket->maciSession, CI_BLOCK_LOCK_FLAG);
    rv = EncryptData(context,pPart, ulPartLen, pEncryptedPart, 
		     *pulEncryptedPartLen);
    MACI_Unlock(socket->maciSession);
    FMUTEX_Unlock(socket->registersLock);

    fort11_FreeSession(session);
    if (rv != SOCKET_SUCCESS) {
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }
    FORT11_RETURN (CKR_OK);
}


/* C_EncryptFinal finishes a multiple-part encryption operation. */
PR_PUBLIC_API(CK_RV) C_EncryptFinal(CK_SESSION_HANDLE hSession,
				    CK_BYTE_PTR       pLastEncryptedPart, 
				    CK_ULONG_PTR      pulLastEncryptedPartLen){
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaContext *context;
    int              rv;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
  
    context = &session->fortezzaContext;

    rv = EndCryptoOperation(context, Encrypt);
    fort11_FreeSession(session);

    FORT11_RETURN (CKR_OK);
}
/* C_DecryptInit initializes a decryption operation. */
PR_PUBLIC_API(CK_RV) C_DecryptInit( CK_SESSION_HANDLE hSession,
				    CK_MECHANISM_PTR  pMechanism, 
				    CK_OBJECT_HANDLE  hKey) {
    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    PK11Object      *keyObject;
    FortezzaSocket  *socket    = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    HSESSION         hs        = socket->maciSession;
    FortezzaKey     *fortezzaKey;
    CI_IV            fortezzaIV;
    int              ciRV, registerIndex;

    if (pMechanism->mechanism != CKM_SKIPJACK_CBC64) {
        if (session) fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);        
    }

    keyObject = fort11_ObjectFromHandle (hKey, session);

    if (keyObject == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    fortezzaKey = (FortezzaKey*)keyObject->objectInfo;
    fort11_FreeObject(keyObject);

    if (fortezzaKey == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    ciRV = MACI_Select (hs, slot->slotID);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    ciRV = MACI_SetMode(hs, CI_DECRYPT_TYPE, CI_CBC64_MODE);
    if (ciRV != CI_OK) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    FMUTEX_Lock(socket->registersLock);
    if (fortezzaKey->keyRegister == KeyNotLoaded) {
        registerIndex = LoadKeyIntoRegister(fortezzaKey);
    } else {
        registerIndex = fortezzaKey->keyRegister;
    }

    if (registerIndex == KeyNotLoaded) {
        FMUTEX_Unlock(socket->registersLock);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    if (pMechanism->pParameter == NULL ||
	pMechanism->ulParameterLen < sizeof (CI_IV)) {
        FORT11_RETURN (CKR_MECHANISM_PARAM_INVALID);
    }

    PORT_Memcpy (fortezzaIV, pMechanism->pParameter, sizeof(CI_IV));

    ciRV = MACI_SetKey (hs, registerIndex);
    if (ciRV != CI_OK) {
        FMUTEX_Unlock(socket->registersLock);
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    ciRV = MACI_LoadIV (hs, fortezzaIV);
    if (ciRV != CI_OK) {
        FMUTEX_Unlock(socket->registersLock);
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    context = &session->fortezzaContext;
    InitContext(context, socket, hKey);
    ciRV = SaveState (context, fortezzaIV, session, fortezzaKey, 
		      CI_DECRYPT_EXT_TYPE, pMechanism->mechanism);

    FMUTEX_Unlock(socket->registersLock);

    if (ciRV != SOCKET_SUCCESS) {
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    InitCryptoOperation (context, Decrypt);
    fort11_FreeSession (session);

    FORT11_RETURN (CKR_OK);
}

/* C_Decrypt decrypts encrypted data in a single part. */
PR_PUBLIC_API(CK_RV) C_Decrypt(CK_SESSION_HANDLE hSession,
			       CK_BYTE_PTR       pEncryptedData,
			       CK_ULONG          ulEncryptedDataLen,
			       CK_BYTE_PTR       pData, 
			       CK_ULONG_PTR      pulDataLen) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle (hSession, PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket  *socket  = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    HSESSION         hs;
    CK_RV            rv;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;

    if (GetCryptoOperation(context) != Decrypt) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_OPERATION_NOT_INITIALIZED);
    }

    *pulDataLen = ulEncryptedDataLen;
    if (pData == NULL) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    hs = socket->maciSession;
    FMUTEX_Lock(socket->registersLock);
    MACI_Lock(hs, CI_NULL_FLAG);
    rv = DecryptData (context, pEncryptedData, ulEncryptedDataLen, 
		      pData, *pulDataLen);
    MACI_Unlock(hs);
    FMUTEX_Unlock(socket->registersLock);
    if (rv != SOCKET_SUCCESS) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    EndCryptoOperation (context, Decrypt);
    fort11_FreeSession(session);

    FORT11_RETURN (CKR_OK);
}


/* C_DecryptUpdate continues a multiple-part decryption operation. */
PR_PUBLIC_API(CK_RV) C_DecryptUpdate(CK_SESSION_HANDLE hSession, 
				     CK_BYTE_PTR       pEncryptedPart, 
				     CK_ULONG          ulEncryptedPartLen,
				     CK_BYTE_PTR       pPart, 
				     CK_ULONG_PTR      pulPartLen) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle(hSession,PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket  *socket  = &fortezzaSockets[slot->slotID-1];
    FortezzaContext *context;
    HSESSION         hs;
    int              rv;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession (session);
        FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;
    hs = socket->maciSession;

    if (GetCryptoOperation(context) != Decrypt) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_OPERATION_NOT_INITIALIZED);
    }

    if (pPart == NULL) {
        *pulPartLen = ulEncryptedPartLen;
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    *pulPartLen = ulEncryptedPartLen;
    
    FMUTEX_Lock(socket->registersLock);
    MACI_Lock (hs, CI_NULL_FLAG);
    rv = DecryptData (context, pEncryptedPart, ulEncryptedPartLen, pPart, 
		      *pulPartLen);
    MACI_Unlock(hs);
    FMUTEX_Unlock(socket->registersLock);

    fort11_FreeSession(session);

    if (rv != SOCKET_SUCCESS) {
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    FORT11_RETURN (CKR_OK);
}


/* C_DecryptFinal finishes a multiple-part decryption operation. */
PR_PUBLIC_API(CK_RV) C_DecryptFinal(CK_SESSION_HANDLE hSession, 
				    CK_BYTE_PTR       pLastPart, 
				    CK_ULONG_PTR      pulLastPartLen) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaContext *context;

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;
    EndCryptoOperation (context, Decrypt);

    fort11_FreeSession(session);

    FORT11_RETURN (CKR_OK);
}


/*
 ************** Crypto Functions:     Digest (HASH)  ************************
 */

/* C_DigestInit initializes a message-digesting operation. */
PR_PUBLIC_API(CK_RV) C_DigestInit(CK_SESSION_HANDLE hSession,
				  CK_MECHANISM_PTR  pMechanism) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_Digest digests data in a single part. */
PR_PUBLIC_API(CK_RV) C_Digest(CK_SESSION_HANDLE hSession, 
			      CK_BYTE_PTR       pData, 
			      CK_ULONG          ulDataLen, 
			      CK_BYTE_PTR       pDigest,
			      CK_ULONG_PTR      pulDigestLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_DigestUpdate continues a multiple-part message-digesting operation. */
PR_PUBLIC_API(CK_RV) C_DigestUpdate(CK_SESSION_HANDLE hSession,
				    CK_BYTE_PTR       pPart,
				    CK_ULONG          ulPartLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_DigestFinal finishes a multiple-part message-digesting operation. */
PR_PUBLIC_API(CK_RV) C_DigestFinal(CK_SESSION_HANDLE hSession,
				   CK_BYTE_PTR       pDigest,
				   CK_ULONG_PTR      pulDigestLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/*
 ************** Crypto Functions:     Sign  ************************
 */

/* C_SignInit initializes a signature (private key encryption) operation,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
PR_PUBLIC_API(CK_RV) C_SignInit(CK_SESSION_HANDLE hSession,
				CK_MECHANISM_PTR  pMechanism, 
				CK_OBJECT_HANDLE  hKey) {
    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle (hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    PK11Object      *keyObject;
    FortezzaSocket  *socket    = &fortezzaSockets[slot->slotID-1]; 
    FortezzaContext *context;
    PK11Attribute   *idAttribute;
    int              personalityIndex;
    HSESSION         hs = socket->maciSession;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (pMechanism->mechanism != CKM_DSA) {
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    keyObject = fort11_ObjectFromHandle (hKey, session);

    if (keyObject == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    context = &session->fortezzaContext;
    InitContext(context, socket, hKey);
    InitCryptoOperation (context, Sign);
    fort11_FreeSession(session);

    idAttribute = fort11_FindAttribute(keyObject, CKA_ID);
    fort11_FreeObject(keyObject);

    if (idAttribute == NULL) {
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    personalityIndex = *(int*)(idAttribute->attrib.pValue);
    fort11_FreeAttribute(idAttribute);
    
    MACI_Select (hs, slot->slotID);
    if (MACI_SetPersonality (hs,personalityIndex) != CI_OK) {
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    FORT11_RETURN (CKR_OK);
}


/* C_Sign signs (encrypts with private key) data in a single part,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
PR_PUBLIC_API(CK_RV) C_Sign(CK_SESSION_HANDLE hSession, 
			    CK_BYTE_PTR       pData,
			    CK_ULONG          ulDataLen,
			    CK_BYTE_PTR       pSignature,
			    CK_ULONG_PTR      pulSignatureLen) {

    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    FortezzaContext *context;
    FortezzaSocket  *socket    = &fortezzaSockets[slot->slotID-1];
    HSESSION         hs        = socket->maciSession;
    PK11Object      *keyObject;
    PK11Attribute   *idAttribute;
    int              ciRV, personalityIndex;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

	
    context = &session->fortezzaContext;
    if (GetCryptoOperation(context) != Sign) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_OPERATION_NOT_INITIALIZED);
    }

    if (pSignature == NULL) {
        *pulSignatureLen = 40;
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    if (ulDataLen > 20) {
        FORT11_RETURN (CKR_DATA_LEN_RANGE);
    }

    if (*pulSignatureLen < 40) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_BUFFER_TOO_SMALL);
    }
    *pulSignatureLen = 40;

    keyObject = fort11_ObjectFromHandle(context->hKey, session);
    if (keyObject == NULL) {
        fort11_FreeSession(session);
	FORT11_RETURN(CKR_GENERAL_ERROR);
    }

    idAttribute = fort11_FindAttribute(keyObject, CKA_ID);
    fort11_FreeObject(keyObject);

    personalityIndex = *(int*)(idAttribute->attrib.pValue);
    fort11_FreeAttribute(idAttribute);

    MACI_Select(hs, slot->slotID);

    MACI_Lock(hs, CI_BLOCK_LOCK_FLAG);
    ciRV = MACI_SetPersonality(hs, personalityIndex);
    if (ciRV != CI_OK) {
        MACI_Unlock(hs);
        fort11_FreeSession(session);
	FORT11_RETURN(CKR_DEVICE_ERROR);
    }

    ciRV = MACI_Sign (hs, pData, pSignature);
    if (ciRV != CI_OK) {
        MACI_Unlock(hs);
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    MACI_Unlock(hs);
    EndCryptoOperation (context, Sign);
    fort11_FreeSession(session);

    FORT11_RETURN (CKR_OK);
}


/* C_SignUpdate continues a multiple-part signature operation,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
PR_PUBLIC_API(CK_RV) C_SignUpdate(CK_SESSION_HANDLE hSession,
				  CK_BYTE_PTR       pPart,
				  CK_ULONG          ulPartLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_SignFinal finishes a multiple-part signature operation, 
 * FORT11_RETURNing the signature. */
PR_PUBLIC_API(CK_RV) C_SignFinal(CK_SESSION_HANDLE hSession,
				 CK_BYTE_PTR       pSignature,
				 CK_ULONG_PTR      pulSignatureLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/*
 ************** Crypto Functions:     Sign Recover  ************************
 */
/* C_SignRecoverInit initializes a signature operation,
 * where the (digest) data can be recovered from the signature. 
 * E.g. encryption with the user's private key */
PR_PUBLIC_API(CK_RV) C_SignRecoverInit(CK_SESSION_HANDLE hSession,
				       CK_MECHANISM_PTR  pMechanism,
				       CK_OBJECT_HANDLE  hKey) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_SignRecover signs data in a single operation
 * where the (digest) data can be recovered from the signature. 
 * E.g. encryption with the user's private key */
PR_PUBLIC_API(CK_RV) C_SignRecover(CK_SESSION_HANDLE hSession,
				   CK_BYTE_PTR       pData,
				   CK_ULONG          ulDataLen, 
				   CK_BYTE_PTR       pSignature, 
				   CK_ULONG_PTR      pulSignatureLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/*
 ************** Crypto Functions:     verify  ************************
 */

/* C_VerifyInit initializes a verification operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature (e.g. DSA) */
PR_PUBLIC_API(CK_RV) C_VerifyInit(CK_SESSION_HANDLE hSession,
				  CK_MECHANISM_PTR  pMechanism,
				  CK_OBJECT_HANDLE  hKey) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_Verify verifies a signature in a single-part operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
PR_PUBLIC_API(CK_RV) C_Verify(CK_SESSION_HANDLE hSession, 
			      CK_BYTE_PTR       pData,
			      CK_ULONG          ulDataLen, 
			      CK_BYTE_PTR       pSignature, 
			      CK_ULONG          ulSignatureLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_VerifyUpdate continues a multiple-part verification operation, 
 * where the signature is an appendix to the data, 
 * and plaintext cannot be recovered from the signature */
PR_PUBLIC_API(CK_RV) C_VerifyUpdate( CK_SESSION_HANDLE hSession, 
				     CK_BYTE_PTR       pPart,
				     CK_ULONG          ulPartLen) {
   return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_VerifyFinal finishes a multiple-part verification operation, 
 * checking the signature. */
PR_PUBLIC_API(CK_RV) C_VerifyFinal(CK_SESSION_HANDLE hSession,
				   CK_BYTE_PTR       pSignature,
				   CK_ULONG          ulSignatureLen) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/*
 ************** Crypto Functions:     Verify  Recover ************************
 */

/* C_VerifyRecoverInit initializes a signature verification operation, 
 * where the data is recovered from the signature. 
 * E.g. Decryption with the user's public key */
PR_PUBLIC_API(CK_RV) C_VerifyRecoverInit(CK_SESSION_HANDLE hSession,
					 CK_MECHANISM_PTR  pMechanism,
					 CK_OBJECT_HANDLE  hKey) {
  /* For functions that don't access globals, we don't have to worry about the
   * stack.
   */	
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_VerifyRecover verifies a signature in a single-part operation, 
 * where the data is recovered from the signature. 
 * E.g. Decryption with the user's public key */
PR_PUBLIC_API(CK_RV) C_VerifyRecover(CK_SESSION_HANDLE hSession,
				     CK_BYTE_PTR       pSignature,
				     CK_ULONG          ulSignatureLen,
				     CK_BYTE_PTR       pData,
				     CK_ULONG_PTR      pulDataLen) {
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/*
 **************************** Key Functions:  ************************
 */

#define MAX_KEY_LEN 256
/* C_GenerateKey generates a secret key, creating a new key object. */
PR_PUBLIC_API(CK_RV) C_GenerateKey(CK_SESSION_HANDLE    hSession, 
				   CK_MECHANISM_PTR     pMechanism,
				   CK_ATTRIBUTE_PTR     pTemplate,
				   CK_ULONG             ulCount,
				   CK_OBJECT_HANDLE_PTR phKey) {
    FORT11_ENTER()
    PK11Session    *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot       *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket *socket  = &fortezzaSockets[slot->slotID-1];
    PK11Object     *key;
    FortezzaKey    *newKey;
    int             i, keyRegister;
    CK_ULONG        key_length = 0;
    CK_RV           crv        = CKR_OK;
    CK_OBJECT_CLASS secretKey  = CKO_SECRET_KEY;
    CK_BBOOL        False      = FALSE;
    CK_BBOOL        cktrue     = TRUE;

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
        fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }
    if (pMechanism->mechanism != CKM_SKIPJACK_KEY_GEN) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    key = fort11_NewObject(slot);

    if (key == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_HOST_MEMORY);
    }

    for (i=0; i < (int) ulCount; i++) {
        if (pTemplate[i].type == CKA_VALUE_LEN) {
	    key_length = *(CK_ULONG *)pTemplate[i].pValue;
	    continue;
	}
	crv = fort11_AddAttributeType (key, pk11_attr_expand (&pTemplate[i]));
	if (crv != CKR_OK) 
	  break;
    } 

    if (crv != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (crv);
    }

    /* make sure we don't have any class, key_type, or value fields */
    fort11_DeleteAttributeType(key,CKA_CLASS);
    fort11_DeleteAttributeType(key,CKA_KEY_TYPE);
    fort11_DeleteAttributeType(key,CKA_VALUE);

    if (MAX_KEY_LEN < key_length) {
        crv = CKR_TEMPLATE_INCONSISTENT;
    }

    if (crv != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (crv);
    }
    
    if (fort11_AddAttributeType(key, CKA_CLASS,&secretKey,
			      sizeof(CK_OBJECT_CLASS)) != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    if (fort11_AddAttributeType(key, CKA_TOKEN, &False,
			      sizeof(CK_BBOOL)) != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }
 
    if (fort11_isTrue(key,CKA_SENSITIVE)) {
	fort11_forceAttribute(key,CKA_ALWAYS_SENSITIVE,&cktrue,
			      sizeof(CK_BBOOL));
    }
    if (!fort11_isTrue(key,CKA_EXTRACTABLE)) {
	fort11_forceAttribute(key,CKA_NEVER_EXTRACTABLE,&cktrue,
			    sizeof(CK_BBOOL));
    }

    FMUTEX_Lock(socket->registersLock);

    keyRegister = GetBestKeyRegister(socket);
    newKey = NewFortezzaKey(socket, MEK, NULL, keyRegister);

    FMUTEX_Unlock(socket->registersLock);

    if (newKey == NULL) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_HOST_MEMORY);
    }

    key->objectInfo = (void*)newKey;
    key->infoFree   = fort11_FreeFortezzaKey;

    FMUTEX_Lock(slot->objectLock);
    key->handle = slot->tokenIDCount++;
    key->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);

    key->objclass = secretKey;
    key->slot = slot;
    key->inDB = PR_TRUE;

    fort11_AddObject(session, key);
    fort11_FreeSession(session);
    SetFortezzaKeyHandle(newKey, key->handle);
    *phKey = key->handle;

    FORT11_RETURN (CKR_OK);
    
}


/* C_GenerateKeyPair generates a public-key/private-key pair, 
 * creating new key objects. */
PR_PUBLIC_API(CK_RV) C_GenerateKeyPair 
                     (CK_SESSION_HANDLE    hSession,
		      CK_MECHANISM_PTR     pMechanism, 
		      CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
		      CK_ULONG             ulPublicKeyAttributeCount, 
		      CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
		      CK_ULONG             ulPrivateKeyAttributeCount, 
		      CK_OBJECT_HANDLE_PTR phPrivateKey,
		      CK_OBJECT_HANDLE_PTR phPublicKey) {
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/* C_WrapKey wraps (i.e., encrypts) a key. */
PR_PUBLIC_API(CK_RV) C_WrapKey(CK_SESSION_HANDLE hSession,
			       CK_MECHANISM_PTR  pMechanism, 
			       CK_OBJECT_HANDLE  hWrappingKey,
			       CK_OBJECT_HANDLE  hKey, 
			       CK_BYTE_PTR       pWrappedKey,
			       CK_ULONG_PTR      pulWrappedKeyLen) {
    FORT11_ENTER()
    PK11Session    *session = fort11_SessionFromHandle (hSession, PR_FALSE);
    PK11Slot       *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket *socket  = &fortezzaSockets[slot->slotID-1];
    PK11Object     *wrapKey;
    PK11Object     *srcKey;
    FortezzaKey    *wrapFortKey;
    FortezzaKey    *srcFortKey;
    int             rv;

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (!socket->isLoggedIn) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
    }

    if (pMechanism->mechanism != CKM_SKIPJACK_WRAP) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    wrapKey = fort11_ObjectFromHandle (hWrappingKey, session);
    if ((wrapKey == NULL) || (wrapKey->objectInfo == NULL)) {
        if (wrapKey) 
	    fort11_FreeObject(wrapKey);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    srcKey = fort11_ObjectFromHandle (hKey, session);
    fort11_FreeSession(session);
    if ((srcKey == NULL) || (srcKey->objectInfo == NULL)) {
        FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    wrapFortKey = (FortezzaKey*)wrapKey->objectInfo;
    fort11_FreeObject(wrapKey);

    srcFortKey = (FortezzaKey*)srcKey->objectInfo;
    fort11_FreeObject(srcKey);

    FMUTEX_Lock(socket->registersLock);
    if (wrapFortKey->keyRegister == KeyNotLoaded) {
      if (LoadKeyIntoRegister(wrapFortKey) == KeyNotLoaded) {
	  FORT11_RETURN (CKR_DEVICE_ERROR);
      }
    }

    if (srcFortKey->keyRegister == KeyNotLoaded) {
      if (LoadKeyIntoRegister(srcFortKey) == KeyNotLoaded) {
	  FMUTEX_Unlock(socket->registersLock);
	  FORT11_RETURN (CKR_DEVICE_ERROR);
      }
    }

    MACI_Lock(socket->maciSession, CI_BLOCK_LOCK_FLAG);
    rv = WrapKey (wrapFortKey, srcFortKey, pWrappedKey, *pulWrappedKeyLen);
    MACI_Unlock(socket->maciSession);
    FMUTEX_Unlock(socket->registersLock);

    if (rv != SOCKET_SUCCESS) {
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    FORT11_RETURN (CKR_OK);
}


/* C_UnwrapKey unwraps (decrypts) a wrapped key, creating a new key object. */
PR_PUBLIC_API(CK_RV) C_UnwrapKey(CK_SESSION_HANDLE    hSession,
				 CK_MECHANISM_PTR     pMechanism, 
				 CK_OBJECT_HANDLE     hUnwrappingKey,
				 CK_BYTE_PTR          pWrappedKey, 
				 CK_ULONG             ulWrappedKeyLen,
				 CK_ATTRIBUTE_PTR     pTemplate, 
				 CK_ULONG             ulAttributeCount,
				 CK_OBJECT_HANDLE_PTR phKey) {
    FORT11_ENTER()
    PK11Session    *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot       *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket *socket  = &fortezzaSockets[slot->slotID-1];
    PK11Object     *wrapKey;
    PK11Object     *newKey;
    FortezzaKey    *fortKey;
    FortezzaKey    *unwrapFort;
    CK_ULONG        key_length;
    int             i, newKeyRegister;
    CK_RV           crv = CKR_OK;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (pMechanism->mechanism != CKM_SKIPJACK_WRAP){
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    if (!socket->isLoggedIn) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_USER_NOT_LOGGED_IN);
    }    

    wrapKey = fort11_ObjectFromHandle(hUnwrappingKey, session);
    if (wrapKey == NULL || wrapKey->objectInfo == NULL) {
        if (wrapKey)
	    fort11_FreeObject(wrapKey);
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_UNWRAPPING_KEY_HANDLE_INVALID);
    }

    fortKey = (FortezzaKey*)wrapKey->objectInfo;
    fort11_FreeObject(wrapKey);

    newKey = fort11_NewObject(slot);
    if (newKey == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_HOST_MEMORY);
    }

    for (i=0; i< (int)ulAttributeCount; i++) {
        if (pTemplate[i].type == CKA_VALUE_LEN) {
	    key_length = *(CK_ULONG*)pTemplate[i].pValue;
	    continue;
	}
	crv=fort11_AddAttributeType(newKey,fort11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) {
	    break;
	}
    }

    if (crv != CKR_OK) {
        fort11_FreeSession(session);
        fort11_FreeObject(newKey);
	FORT11_RETURN (crv);
    }

    /* make sure we don't have any class, key_type, or value fields */
    if (!fort11_hasAttribute(newKey,CKA_CLASS)) {
	fort11_FreeObject(newKey);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_TEMPLATE_INCOMPLETE);
    }
    if (!fort11_hasAttribute(newKey,CKA_KEY_TYPE)) {
	fort11_FreeObject(newKey);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_TEMPLATE_INCOMPLETE);
    }

    FMUTEX_Lock(socket->registersLock);
    newKeyRegister = UnwrapKey (pWrappedKey, fortKey);
    if (newKeyRegister == KeyNotLoaded) {
        /*Couldn't Unwrap the key*/
        fort11_FreeObject(newKey);
        fort11_FreeSession(session);
	FMUTEX_Unlock(socket->registersLock);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    unwrapFort = NewUnwrappedKey(newKeyRegister, fortKey->id, socket);
    FMUTEX_Unlock(socket->registersLock);

    if (unwrapFort == NULL) {
        fort11_FreeObject(newKey);
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_HOST_MEMORY);
    }
    newKey->objectInfo = unwrapFort;
    newKey->infoFree   = fort11_FreeFortezzaKey;

    FMUTEX_Lock(slot->objectLock);
    newKey->handle = slot->tokenIDCount++;
    newKey->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);
    newKey->objclass = CKO_SECRET_KEY;
    newKey->slot  = slot;
    newKey->inDB  = PR_TRUE;

    fort11_AddObject (session, newKey);
    fort11_FreeSession(session);
    
    SetFortezzaKeyHandle(unwrapFort, newKey->handle);
    *phKey = newKey->handle;

    FORT11_RETURN (CKR_OK);
}


/* C_DeriveKey derives a key from a base key, creating a new key object. */
PR_PUBLIC_API(CK_RV) C_DeriveKey( CK_SESSION_HANDLE    hSession,
				  CK_MECHANISM_PTR     pMechanism, 
				  CK_OBJECT_HANDLE     hBaseKey,
				  CK_ATTRIBUTE_PTR     pTemplate, 
				  CK_ULONG             ulAttributeCount, 
				  CK_OBJECT_HANDLE_PTR phKey) {
    FORT11_ENTER()
    PK11Session     *session = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot    = fort11_SlotFromSessionHandle(hSession);
    FortezzaSocket  *socket  = &fortezzaSockets[slot->slotID-1];
    PK11Object      *key, *sourceKey;
    CK_ULONG         i;
    CK_ULONG         key_length = 0;
    CK_RV            crv        = 0;
    CK_KEY_TYPE      keyType    = CKK_SKIPJACK;
    CK_OBJECT_CLASS  classType  = CKO_SECRET_KEY;  
    CK_BBOOL	     ckTrue     = TRUE;
    CK_BBOOL         ckFalse    = FALSE;
    int              ciRV;
    int              personality;
    PK11Attribute   *att;

    CK_KEA_DERIVE_PARAMS_PTR  params;
    FortezzaKey              *derivedKey;
    CreateTEKInfo             tekInfo;

    if (session == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (pMechanism->mechanism != CKM_KEA_KEY_DERIVE) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_INVALID);
    }

    key = fort11_NewObject (slot);

    if (key == NULL) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_HOST_MEMORY);
    }

    for (i = 0; i < ulAttributeCount; i++) {
        crv = fort11_AddAttributeType (key, fort11_attr_expand(&pTemplate[i]));
	if (crv != CKR_OK) {
	    break;
	}
	if (pTemplate[i].type == CKA_KEY_TYPE) {
	    keyType = *(CK_KEY_TYPE*)pTemplate[i].pValue;
	} else if (pTemplate[i].type == CKA_VALUE_LEN) {
	    key_length = *(CK_ULONG*)pTemplate[i].pValue;
	}	  
    }

    if (crv != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (crv);
    }

    if (key_length == 0) {
        key_length = 12;
    }

    classType = CKO_SECRET_KEY;
    crv = fort11_forceAttribute (key, CKA_CLASS, &classType, 
				 sizeof(classType));
    if (crv != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (crv);
    }
    crv = fort11_forceAttribute (key, CKA_SENSITIVE, &ckTrue, 
				 sizeof(CK_BBOOL));
    if (crv != CKR_OK) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (crv);
    }
    crv = fort11_forceAttribute (key, CKA_EXTRACTABLE, &ckFalse, 
			       sizeof(CK_BBOOL));
    if (crv != CKR_OK) {
        fort11_FreeSession(session);
	fort11_FreeObject(key);
	FORT11_RETURN (crv);
    }

    sourceKey = fort11_ObjectFromHandle (hBaseKey, session);

    if (sourceKey == NULL) {
        fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
    }

    att = fort11_FindAttribute(sourceKey,CKA_ID);
    fort11_FreeObject(sourceKey);
    if (att == NULL) {
	fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_KEY_TYPE_INCONSISTENT);
    }
    personality = *(int *) att->attrib.pValue;
    fort11_FreeAttribute(att);

    params = (CK_KEA_DERIVE_PARAMS_PTR)pMechanism->pParameter;

    if (params == NULL) {
	fort11_FreeObject(key);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_MECHANISM_PARAM_INVALID);
    }
    
    ciRV = MACI_SetPersonality(socket->maciSession,personality);
    if (ciRV != CI_OK) {
	fort11_FreeObject(key);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }
    /*
     * If we're sending, generate our own RA.
     */
    if (params->isSender) {
	ciRV = MACI_GenerateRa(socket->maciSession,params->pRandomA);
	if (ciRV != CI_OK) {
	    fort11_FreeObject(key);
	    fort11_FreeSession(session);
	    FORT11_RETURN (CKR_DEVICE_ERROR);
	}
    }
    PORT_Memcpy (tekInfo.Ra, params->pRandomA, params->ulRandomLen);
    PORT_Memcpy (tekInfo.Rb, params->pRandomB, params->ulRandomLen);
    tekInfo.randomLen = params->ulRandomLen;
    tekInfo.personality = personality;
    tekInfo.flag = (params->isSender) ? CI_INITIATOR_FLAG : CI_RECIPIENT_FLAG;
    
    PORT_Memcpy (tekInfo.pY, params->pPublicData, params->ulPublicDataLen);
    tekInfo.YSize = params->ulPublicDataLen;

    FMUTEX_Lock(socket->registersLock);
    derivedKey = NewFortezzaKey(socket, TEK, &tekInfo, 
				GetBestKeyRegister(socket));
    FMUTEX_Unlock(socket->registersLock);

    if (derivedKey == NULL) {
	fort11_FreeObject(key);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_GENERAL_ERROR);
    }

    key->objectInfo = derivedKey;
    key->infoFree   = fort11_FreeFortezzaKey;

    FMUTEX_Lock(slot->objectLock);
    key->handle = slot->tokenIDCount++;
    key->handle |= (PK11_TOKEN_MAGIC | PK11_TOKEN_TYPE_PRIV);
    FMUTEX_Unlock(slot->objectLock);
    key->objclass = classType;
    key->slot = slot;
    key->inDB = PR_TRUE;

    fort11_AddObject (session, key);
    fort11_FreeSession(session);
    
    SetFortezzaKeyHandle(derivedKey, key->handle);
    *phKey = key->handle;

    FORT11_RETURN (CKR_OK);
}

/*
 **************************** Random Functions:  ************************
 */

/* C_SeedRandom mixes additional seed material into the token's random number 
 * generator. */
PR_PUBLIC_API(CK_RV) C_SeedRandom(CK_SESSION_HANDLE hSession, 
				  CK_BYTE_PTR       pSeed,
				  CK_ULONG          ulSeedLen) {
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_GenerateRandom generates random data. */
PR_PUBLIC_API(CK_RV) C_GenerateRandom(CK_SESSION_HANDLE hSession,
				      CK_BYTE_PTR	pRandomData, 
				      CK_ULONG          ulRandomLen) {
    FORT11_ENTER()
    PK11Slot    *slot    = fort11_SlotFromSessionHandle(hSession);
    PK11Session *session = fort11_SessionFromHandle(hSession,PR_FALSE);
    CI_RANDOM    randomNum;
    CK_ULONG     randomSize = sizeof (CI_RANDOM);
    int          ciRV;
    CK_ULONG     bytesCopied = 0, bytesToCopy;
    CK_ULONG     bufferSize  = 0, bytesRemaining;

    if (session == NULL) {
        session = fort11_SessionFromHandle (hSession, PR_TRUE);
	fort11_TokenRemoved(slot, session);
	fort11_FreeSession(session);
        FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    fort11_FreeSession(session);
    ciRV = MACI_Select(fortezzaSockets[slot->slotID-1].maciSession, 
		       slot->slotID);
    if (ciRV != CI_OK) {
        FORT11_RETURN (CKR_DEVICE_ERROR);
    }
    
    while (bytesCopied < ulRandomLen) {
      bytesRemaining = ulRandomLen - bytesCopied;
      if (bufferSize < bytesRemaining) {
	  ciRV = 
	    MACI_GenerateRandom(fortezzaSockets[slot->slotID-1].maciSession, 
				randomNum);
	  if (ciRV != CI_OK)
	      FORT11_RETURN (CKR_DEVICE_ERROR);
	  bufferSize = randomSize;
      }
      bytesToCopy = (bytesRemaining > randomSize) ? randomSize : 
	                                            bytesRemaining;

      PORT_Memcpy (&pRandomData[bytesCopied], 
		   &randomNum[randomSize-bufferSize], bytesToCopy);

      bytesCopied += bytesToCopy;
      bufferSize  -= bytesToCopy; 
    }

    FORT11_RETURN (CKR_OK);
}


/* C_GetFunctionStatus obtains an updated status of a function running 
 * in parallel with an application. */
PR_PUBLIC_API(CK_RV) C_GetFunctionStatus(CK_SESSION_HANDLE hSession) {
    return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_CancelFunction cancels a function running in parallel */
PR_PUBLIC_API(CK_RV) C_CancelFunction(CK_SESSION_HANDLE hSession) {
    return CKR_FUNCTION_NOT_SUPPORTED;
}

/* C_GetOperationState saves the state of the cryptographic 
 *operation in a session. */
PR_PUBLIC_API(CK_RV) C_GetOperationState(CK_SESSION_HANDLE hSession, 
					 CK_BYTE_PTR       pOperationState, 
					 CK_ULONG_PTR      pulOperationStateLen) {
    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    FortezzaContext *context;

    if (session  == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (pOperationState == NULL) {
        *pulOperationStateLen = sizeof (FortezzaContext);
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_OK);
    }

    if (*pulOperationStateLen < sizeof (FortezzaContext)) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_BUFFER_TOO_SMALL);
    }

    context = &session->fortezzaContext;
    fort11_FreeSession(session);
    PORT_Memcpy (pOperationState, context, sizeof(FortezzaContext));
    ((FortezzaContext *)pOperationState)->session = NULL;
    ((FortezzaContext *)pOperationState)->fortezzaKey = NULL;
    *pulOperationStateLen = sizeof(FortezzaContext);
    FORT11_RETURN (CKR_OK);
}



/* C_SetOperationState restores the state of the cryptographic operation in a session. */
PR_PUBLIC_API(CK_RV) C_SetOperationState(CK_SESSION_HANDLE hSession, 
					 CK_BYTE_PTR       pOperationState, 
					 CK_ULONG          ulOperationStateLen,
					 CK_OBJECT_HANDLE  hEncryptionKey, 
					 CK_OBJECT_HANDLE  hAuthenticationKey){
    FORT11_ENTER()
    PK11Session     *session   = fort11_SessionFromHandle(hSession, PR_FALSE);
    PK11Slot        *slot      = fort11_SlotFromSessionHandle(hSession);
    FortezzaContext *context;
    FortezzaContext  passedInCxt;
    PK11Object      *keyObject;
    FortezzaKey     *fortKey;

    if (session  == NULL) {
        session = fort11_SessionFromHandle(hSession, PR_TRUE);
	fort11_TokenRemoved (slot, session);
	fort11_FreeSession(session);
	FORT11_RETURN (CKR_SESSION_HANDLE_INVALID);
    }

    if (ulOperationStateLen != sizeof(FortezzaContext)) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_SAVED_STATE_INVALID);
    }

    PORT_Memcpy(&passedInCxt, pOperationState, sizeof(FortezzaContext));
    if (passedInCxt.fortezzaSocket->slotID != slot->slotID) {
        fort11_FreeSession(session);
        FORT11_RETURN (CKR_SAVED_STATE_INVALID);
    }
    passedInCxt.session = NULL;
    passedInCxt.fortezzaKey = NULL;
    
    if (hEncryptionKey != 0) {
        keyObject = fort11_ObjectFromHandle(hEncryptionKey, session);
	if (keyObject == NULL) {
	    fort11_FreeSession(session);
	    FORT11_RETURN (CKR_KEY_HANDLE_INVALID);
	}
	fortKey = (FortezzaKey*)keyObject->objectInfo;
	fort11_FreeObject(keyObject);
	if (fortKey == NULL) {
	    fort11_FreeSession(session);
	    FORT11_RETURN (CKR_SAVED_STATE_INVALID);
	}
	if (fortKey->keyRegister == KeyNotLoaded) {
	    if (LoadKeyIntoRegister (fortKey) == KeyNotLoaded) {
	        fort11_FreeSession(session);
		FORT11_RETURN (CKR_DEVICE_ERROR);
	    }
	}
	passedInCxt.fortezzaKey = fortKey;

    } 
    if (hAuthenticationKey != 0) {
        fort11_FreeSession(session);
	FORT11_RETURN (CKR_DEVICE_ERROR);
    }

    passedInCxt.session = session;
    context = &session->fortezzaContext;
    fort11_FreeSession (session);    
    PORT_Memcpy (context, &passedInCxt, sizeof(passedInCxt));

    FORT11_RETURN (CKR_OK);
}

/* Dual-function cryptographic operations */

/* C_DigestEncryptUpdate continues a multiple-part digesting and 
   encryption operation. */
PR_PUBLIC_API(CK_RV) C_DigestEncryptUpdate(CK_SESSION_HANDLE hSession, 
					   CK_BYTE_PTR       pPart,
					   CK_ULONG          ulPartLen, 
					   CK_BYTE_PTR       pEncryptedPart,
					   CK_ULONG_PTR      pulEncryptedPartLen){
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_DecryptDigestUpdate continues a multiple-part decryption and digesting 
   operation. */
PR_PUBLIC_API(CK_RV) C_DecryptDigestUpdate(CK_SESSION_HANDLE hSession,
					   CK_BYTE_PTR       pEncryptedPart, 
					   CK_ULONG          ulEncryptedPartLen,
					   CK_BYTE_PTR       pPart, 
			    CK_ULONG_PTR      pulPartLen){
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_SignEncryptUpdate continues a multiple-part signing and encryption
   operation. */
PR_PUBLIC_API(CK_RV) C_SignEncryptUpdate(CK_SESSION_HANDLE hSession, 
					 CK_BYTE_PTR       pPart,
					 CK_ULONG          ulPartLen, 
					 CK_BYTE_PTR       pEncryptedPart,
					 CK_ULONG_PTR      pulEncryptedPartLen){
  return CKR_FUNCTION_NOT_SUPPORTED;
}


/* C_DecryptVerifyUpdate continues a multiple-part decryption and verify 
   operation. */
PR_PUBLIC_API(CK_RV) C_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession, 
					   CK_BYTE_PTR       pEncryptedData, 
					   CK_ULONG          ulEncryptedDataLen, 
					   CK_BYTE_PTR       pData, 
					   CK_ULONG_PTR      pulDataLen){
  return CKR_FUNCTION_NOT_SUPPORTED;
}

/* C_DigestKey continues a multi-part message-digesting operation,
 * by digesting the value of a secret key as part of the data already digested.
 */
PR_PUBLIC_API(CK_RV) C_DigestKey(CK_SESSION_HANDLE hSession, 
				 CK_OBJECT_HANDLE  hKey) {
  return CKR_FUNCTION_NOT_SUPPORTED;
}

PR_PUBLIC_API(CK_RV) C_WaitForSlotEvent(CK_FLAGS flags,
					CK_SLOT_ID_PTR pSlot,
					CK_VOID_PTR pRserved) {
  return CKR_FUNCTION_FAILED;
}

