/*
 * pk11.c - SVRCORE module for securely storing PIN using PK11
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
 * The Original Code is the Netscape svrcore library.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include <svrcore.h>

#include <string.h>
#include <secitem.h>
#include <pk11func.h>

/* ------------------------------------------------------------ */
/*
 * Mechanisms for doing the PIN encryption.  Each of these lists
 * an encryption mechanism, with setup, encode and decode routines that
 * use that mechanism.  The PK11PinStore looks for a mechanism
 * that the token supports, and then uses it.  If none is found,
 * it will fail.
 */
typedef struct mech_item mech_item;
struct mech_item
{
  CK_MECHANISM_TYPE type;
  const char *mechName;
};

/* ------------------------------------------------------------ */
/*
 * The table listing all mechanism to try
 */
#define MECH_TABLE_SIZE 4
static const mech_item table[MECH_TABLE_SIZE] = {
  { CKM_SKIPJACK_CBC64, "Skipjack CBC-64 encryption" },
  { CKM_DES3_CBC,       "Triple-DES CBC encryption" },
  { CKM_CAST128_CBC,    "CAST-128 CBC encryption" },
  { CKM_DES_CBC,        "DES CBC encryption" }
};
static mech_item dflt_mech = { CKM_DES3_CBC, "Triple-DES CBC (default)" };


/* ------------------------------------------------------------ */
/*
 * Implementation
 */
struct SVRCOREPk11PinStore
{
  PK11SlotInfo *slot;

  const mech_item *mech;

  PK11SymKey *key;
  SECItem *params;

  int length;
  unsigned char *crypt;
};


/* ------------------------------------------------------------ */
/*
 * SVRCORE_CreatePk11PinStore
 */
SVRCOREError
SVRCORE_CreatePk11PinStore(
  SVRCOREPk11PinStore **out,
  const char *tokenName, const char *pin)
{
  SVRCOREError err;
  SVRCOREPk11PinStore *store;

  do {
    err = SVRCORE_Success;

    store = (SVRCOREPk11PinStore*)malloc(sizeof *store);
    if (store == 0) { err = SVRCORE_NoMemory_Error; break; }

    /* Low-level init */
    store->slot = 0;
    store->key = 0;
    store->params = 0;
    store->crypt = 0;

    /* Use the tokenName to find a PKCS11 slot */
    store->slot = PK11_FindSlotByName((char *)tokenName);
    if (store->slot == 0) { err = SVRCORE_NoSuchToken_Error; break; }

    /* Check the password/PIN.  This allows access to the token */
    {
      SECStatus rv = PK11_CheckUserPassword(store->slot, (char *)pin);

      if (rv == SECSuccess)
        ;
      else if (rv == SECWouldBlock)
      {
        err = SVRCORE_IncorrectPassword_Error;
        break;
      }
      else
      {
        err = SVRCORE_System_Error;
        break;
      }
    }

    /* Find the mechanism that this token can do */
    {
      const mech_item *tp;

      store->mech = 0;
      for(tp = table;tp < &table[MECH_TABLE_SIZE];tp++)
      {
        if (PK11_DoesMechanism(store->slot, tp->type))
        {
          store->mech = tp;
          break;
        }
      }
      /* Default to a mechanism (probably on the internal token */
      if (store->mech == 0)
        store->mech = &dflt_mech;
    }

    /* Generate a key and parameters to do the encryption */
    store->key = PK11_KeyGen(store->slot, store->mech->type,
                   0, 0, 0);
    if (store->key == 0)
    {
      /* PR_SetError(xxx); */
      err = SVRCORE_System_Error;
      break;
    }

    store->params = PK11_GenerateNewParam(store->mech->type, store->key);
    if (store->params == 0)
    {
      err = SVRCORE_System_Error;
      break;
    }

    /* Compute the size of the encrypted data including necessary padding */
    {
      int blocksize = PK11_GetBlockSize(store->mech->type, 0);

      store->length = strlen(pin)+1;

      /* Compute padded size - 0 means stream cipher */
      if (blocksize != 0)
      {
        store->length += blocksize - (store->length % blocksize);
      }

      store->crypt = (unsigned char *)malloc(store->length);
      if (!store->crypt) { err = SVRCORE_NoMemory_Error; break; }
    }

    /* Encrypt */
    {
      unsigned char *plain;
      PK11Context *ctx;
      SECStatus rv;
      int outLen;

      plain = (unsigned char *)malloc(store->length);
      if (!plain) { err = SVRCORE_NoMemory_Error; break; }

      /* Pad with 0 bytes */
      memset(plain, 0, store->length);
      strcpy((char *)plain, pin);

      ctx = PK11_CreateContextBySymKey(store->mech->type, CKA_ENCRYPT,
              store->key, store->params);
      if (!ctx) { err = SVRCORE_System_Error; break; }

      do {
        rv = PK11_CipherOp(ctx, store->crypt, &outLen, store->length,
               plain, store->length);
        if (rv) break;

        rv = PK11_Finalize(ctx);
      } while(0);

      PK11_DestroyContext(ctx, PR_TRUE);
      memset(plain, 0, store->length);
      free(plain);

      if (rv) err = SVRCORE_System_Error;
    }
  } while(0);

  if (err)
  {
    SVRCORE_DestroyPk11PinStore(store);
    store = 0;
  }

  *out = store;
  return err;
}

/*
 * SVRCORE_DestroyPk11PinStore
 */
void
SVRCORE_DestroyPk11PinStore(SVRCOREPk11PinStore *store)
{
  if (store == 0) return;

  if (store->slot)
  {
    PK11_FreeSlot(store->slot);
  }

  if (store->params)
  {
    SECITEM_ZfreeItem(store->params, PR_TRUE);
  }

  if (store->key)
  {
    PK11_FreeSymKey(store->key);
  }

  if (store->crypt)
  {
    memset(store->crypt, 0, store->length);
    free(store->crypt);
  }

  free(store);
}

SVRCOREError
SVRCORE_Pk11StoreGetPin(char **out, SVRCOREPk11PinStore *store)
{
  SVRCOREError err = SVRCORE_Success;
  unsigned char *plain;
  SECStatus rv;
  PK11Context *ctx = 0;
  int outLen;

  do {
    plain = (unsigned char *)malloc(store->length);
    if (!plain) { err = SVRCORE_NoMemory_Error; break; }

    ctx = PK11_CreateContextBySymKey(store->mech->type, CKA_DECRYPT,
              store->key, store->params);
    if (!ctx) { err = SVRCORE_System_Error; break; }

    rv = PK11_CipherOp(ctx, plain, &outLen, store->length,
           store->crypt, store->length);
    if (rv) break;

    rv = PK11_Finalize(ctx);
    if (rv) break;
  } while(0);

  if (ctx) PK11_DestroyContext(ctx, PR_TRUE);

  if (rv)
  {
    err = SVRCORE_System_Error;
    memset(plain, 0, store->length);
    free(plain);
    plain = 0;
  }

  *out = (char *)plain;
  return err;
}

const char *
SVRCORE_Pk11StoreGetMechName(const SVRCOREPk11PinStore *store)
{
  return store->mech->mechName;
}
