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
#ifndef _context_h_
#define _context_h_

#ifdef SWFORT
#ifndef RETURN_TYPE
#define RETURN_TYPE int
#endif
#endif
#include "cryptint.h"
#include "genci.h"
#include "maci.h"

typedef enum {NOKEY, TEK, MEK, UNWRAP, Ks} FortezzaKeyType;
typedef enum {Encrypt, Decrypt, Sign, None} CryptoType;

typedef struct FortezzaKeyStr *FortezzaKeyPtr;
typedef struct FortezzaSocketStr *FortezzaSocketPtr;
typedef struct FortezzaKeyStr FortezzaKey;
typedef unsigned char FortezzaMEK[12]; 


typedef struct CreateTEKInfoStr {
    CI_RA         Ra;
    CI_RB         Rb;
    unsigned long randomLen;
    int		  personality;
    int           flag; /*Either CI_INITIATOR_FLAG or CI_RECIPIENT_FLAG*/ 
    CI_Y          pY;
    unsigned int  YSize; 
} CreateTEKInfo;

typedef struct FortezzaTEKStr {
    CI_RA  Ra;  /*All the parameters necessary to create a TEK */
    CI_RB  Rb;
    unsigned long randomLen;
    CI_Y   pY;
    int    flags;
    int    registerIndex;
    unsigned int  ySize;
} FortezzaTEK;

struct FortezzaKeyStr {
    FortezzaKeyPtr    next, prev;
    CK_OBJECT_HANDLE  keyHandle;
    int               keyRegister;
    FortezzaKeyType   keyType;
    FortezzaSocketPtr keySocket;
    unsigned long     id;
    unsigned long     hitCount;
    union {
        FortezzaTEK tek;
        FortezzaMEK mek;
    } keyData;
};

typedef struct FortezzaSocketStr {
    PRBool            isOpen;
    PRBool            isLoggedIn;
    PRBool            hasLoggedIn;
    PRBool            personalitiesLoaded;
    unsigned long     slotID;
    unsigned long     hitCount;
    HSESSION          maciSession;
    CI_SERIAL_NUMBER  openCardSerial;
    CI_STATE          openCardState;
    CI_PERSON        *personalityList;
    int               numPersonalities;
    int               numKeyRegisters;
    FortezzaKey     **keyRegisters; /*Array of pointers to keys in registers*/
    FortezzaKey      *keys;         /*Linked list of all the keys*/
    void             *registersLock;
} FortezzaSocket;

typedef struct PK11SessionStr *PK11SessionPtr;

typedef struct FortezzaConstextStr {
    FortezzaKey         *fortezzaKey;
    FortezzaSocket      *fortezzaSocket;
    PK11SessionPtr       session;
    CryptoType           cryptoOperation;
    CK_MECHANISM_TYPE    mechanism;
    CI_SAVE_DATA         cardState;
    CI_IV                cardIV;
    unsigned long        userRamSize;
    CK_OBJECT_HANDLE     hKey;
} FortezzaContext;



#endif /*_context_h_*/
