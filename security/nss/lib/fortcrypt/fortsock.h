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
#ifndef FORSOCK_H_
#define FORSOCK_H_

#include "seccomon.h"
#include "fpkcs11.h"
#include "fpkcs11i.h"
#include "fpkstrs.h"


#ifndef prtypes_h___
typedef enum { PR_FALSE, PR_TRUE }PRBool;
#endif


#define SOCKET_SUCCESS  0
#define SOCKET_FAILURE  1

#define KeyNotLoaded    -1
#define NoCryptoType    -1
#define NoCryptoMode    -1
#define NO_MECHANISM    0xFFFFFFFFL


/*Get the Fortezza context in here*/

int InitSocket (FortezzaSocket *inSocket, int inSlotID);
int FreeSocket (FortezzaSocket *inSocket);

int FetchPersonalityList (FortezzaSocket *inSocket);
int UnloadPersonalityList(FortezzaSocket *inSocket);

int LoginToSocket (FortezzaSocket *inSocket, int inUserType, CI_PIN inPin); 

int LogoutFromSocket (FortezzaSocket *inSocket);

PRBool SocketStateUnchanged(FortezzaSocket* inSocket);

int GetBestKeyRegister(FortezzaSocket *inSocket);

FortezzaKey *NewFortezzaKey(FortezzaSocket  *inSocket, 
			    FortezzaKeyType  inKeyType,
			    CreateTEKInfo   *TEKinfo,
			    int              inKeyRegister);
FortezzaKey *NewUnwrappedKey(int inKeyRegister, int i, 
			     FortezzaSocket *inSocket);

int  LoadKeyIntoRegister (FortezzaKey *inKey);
int  SetFortezzaKeyHandle (FortezzaKey *inKey, CK_OBJECT_HANDLE inHandle);
void RemoveKey (FortezzaKey *inKey);

void InitContext(FortezzaContext *inContext, FortezzaSocket *inSocket,
		 CK_OBJECT_HANDLE hKey);
int InitCryptoOperation (FortezzaContext *inContext, 
			 CryptoType inCryptoOperation);
int EndCryptoOperation  (FortezzaContext *inContext, 
			 CryptoType inCryptoOperation);
CryptoType GetCryptoOperation (FortezzaContext *inContext);
int EncryptData (FortezzaContext *inContext, CK_BYTE_PTR inData,
		 CK_ULONG inDataLen, CK_BYTE_PTR inDest, 
		 CK_ULONG inDestLen);
int DecryptData (FortezzaContext *inContext, CK_BYTE_PTR inData,
		 CK_ULONG inDataLen, CK_BYTE_PTR inDest, 
		 CK_ULONG inDestLen);

int SaveState (FortezzaContext *inContext, CI_IV inIV, 
	       PK11Session *inSession, FortezzaKey *inKey,
	       int inCryptoType, CK_MECHANISM_TYPE inMechanism);

int WrapKey (FortezzaKey *wrappingKey, FortezzaKey *srcKey,
	     CK_BYTE_PTR pDest, CK_ULONG ulDestLen);
int UnwrapKey (CK_BYTE_PTR inWrappedKey, FortezzaKey *inKey);

#endif /*SOCKET_H_*/
