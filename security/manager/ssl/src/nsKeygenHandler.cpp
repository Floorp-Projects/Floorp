/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

extern "C" {
#include "secdert.h"
#include "keydbt.h"
}
#include "nspr.h"
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "keyhi.h"
#include "secder.h"
#include "cryptohi.h"
#include "base64.h"
#include "secasn1.h"
#include "nsProxiedService.h"
#include "nsKeygenHandler.h"
#include "nsVoidArray.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIContent.h"
#include "nsINSSDialogs.h"
#include "nsKeygenThread.h"
#include "nsReadableUtils.h"

//These defines are taken from the PKCS#11 spec
#define CKM_RSA_PKCS_KEY_PAIR_GEN     0x00000000
#define CKM_DH_PKCS_KEY_PAIR_GEN      0x00000020
#define CKM_DSA_KEY_PAIR_GEN          0x00000010

//All possible key size choices.
static SECKeySizeChoiceInfo SECKeySizeChoiceList[] = {
    { nsnull, 2048 },
    { nsnull, 1024 },
    { nsnull, 512  },
    { nsnull, 0 }, 
};


DERTemplate CERTSubjectPublicKeyInfoTemplate[] = {
    { DER_SEQUENCE,
          0, nsnull, sizeof(CERTSubjectPublicKeyInfo) },
    { DER_INLINE,
          offsetof(CERTSubjectPublicKeyInfo,algorithm),
          SECAlgorithmIDTemplate, },
    { DER_BIT_STRING,
          offsetof(CERTSubjectPublicKeyInfo,subjectPublicKey), },
    { 0, }
};

DERTemplate CERTPublicKeyAndChallengeTemplate[] =
{
    { DER_SEQUENCE, 0, nsnull, sizeof(CERTPublicKeyAndChallenge) },
    { DER_ANY, offsetof(CERTPublicKeyAndChallenge,spki), },
    { DER_IA5_STRING, offsetof(CERTPublicKeyAndChallenge,challenge), },
    { 0, }
};

static NS_DEFINE_IID(kFormProcessorIID,   NS_IFORMPROCESSOR_IID); 
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

static const char *mozKeyGen = "-mozilla-keygen";

static PQGParams *
decode_pqg_params(char *aStr)
{
    unsigned char *buf;
    unsigned int len;
    PRArenaPool *arena;
    PQGParams *params;
    SECStatus status;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return nsnull;

    params = NS_STATIC_CAST(PQGParams*, PORT_ArenaZAlloc(arena, sizeof(PQGParams)));
    if (!params)
        goto loser;
    params->arena = arena;

    buf = ATOB_AsciiToData(aStr, &len);
    if ((!buf) || (len == 0))
        goto loser;

    status = SEC_ASN1Decode(arena, params, SECKEY_PQGParamsTemplate, (const char*)buf, len);
    if (status != SECSuccess)
        goto loser;

    return params;

loser:
    if (arena) {
      PORT_FreeArena(arena, PR_FALSE);
    }
    if (buf) {
      PR_Free(buf);
    }
    return nsnull;
}

static int
pqg_prime_bits(char *str)
{
    PQGParams *params = nsnull;
    int primeBits = 0, i;

    params = decode_pqg_params(str);
    if (!params)
        goto done; /* lose */

    for (i = 0; params->prime.data[i] == 0; i++)
        /* empty */;
    primeBits = (params->prime.len - i) * 8;

done:
    if (params)
        PQG_DestroyParams(params);
    return primeBits;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsKeygenFormProcessor, nsIFormProcessor);
MOZ_DECL_CTOR_COUNTER(nsKeygenFormProcessor)

nsKeygenFormProcessor::nsKeygenFormProcessor()
{ 
   NS_INIT_REFCNT();
   MOZ_COUNT_CTOR(nsKeygenFormProcessor);
   m_ctx = new PipUIContext();

} 

nsKeygenFormProcessor::~nsKeygenFormProcessor()
{
  MOZ_COUNT_DTOR(nsKeygenFormProcessor);
}

NS_METHOD
nsKeygenFormProcessor::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
  nsresult rv;
  NS_ENSURE_NO_AGGREGATION(aOuter);
  nsKeygenFormProcessor* formProc = new nsKeygenFormProcessor();
  if (!formProc)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsISupports> stabilize = formProc;
  rv = formProc->Init();
  if (NS_SUCCEEDED(rv)) {
    rv = formProc->QueryInterface(aIID, aResult);
  }
  return rv;
}

nsresult
nsKeygenFormProcessor::Init()
{
  nsresult rv;
  nsAutoString str;

  // Get the key strings //
  nsCOMPtr<nsINSSComponent> nssComponent;
  nssComponent = do_GetService(kNSSComponentCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("HighGrade").get(),
                            str);
  SECKeySizeChoiceList[0].name = ToNewUnicode(str);

  nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("MediumGrade").get(),
                            str);
  SECKeySizeChoiceList[1].name = ToNewUnicode(str);

  nssComponent->GetPIPNSSBundleString(
                            NS_LITERAL_STRING("LowGrade").get(),
                            str);
  SECKeySizeChoiceList[2].name = ToNewUnicode(str);

  return NS_OK;
}

nsresult
nsKeygenFormProcessor::GetSlot(PRUint32 aMechanism, PK11SlotInfo** aSlot)
{
  return GetSlotWithMechanism(aMechanism,m_ctx,aSlot);
}


PRUint32 MapGenMechToAlgoMech(PRUint32 mechanism)
{
    PRUint32 searchMech;

    /* We are interested in slots based on the ability to perform
       a given algorithm, not on their ability to generate keys usable
       by that algorithm. Therefore, map keygen-specific mechanism tags
       to tags for the corresponding crypto algorthm. */
    switch(mechanism)
    {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
        searchMech = CKM_RSA_PKCS;
        break;
    case CKM_DSA_KEY_PAIR_GEN:
        searchMech = CKM_DSA;
        break;
    case CKM_RC4_KEY_GEN:
        searchMech = CKM_RC4;
        break;
    case CKM_DH_PKCS_KEY_PAIR_GEN:
        searchMech = CKM_DH_PKCS_DERIVE; /* ### mwelch  is this right? */
        break;
    case CKM_DES_KEY_GEN:
        /* What do we do about DES keygen? Right now, we're just using
           DES_KEY_GEN to look for tokens, because otherwise we'll have
           to search the token list three times. */
    default:
        searchMech = mechanism;
        break;
    }
    return searchMech;
}


nsresult
GetSlotWithMechanism(PRUint32 aMechanism, 
                     nsIInterfaceRequestor *m_ctx,
                     PK11SlotInfo** aSlot)
{
    PK11SlotList * slotList = nsnull;
    PRUnichar** tokenNameList = nsnull;
    nsITokenDialogs * dialogs;
    PRUnichar *unicodeTokenChosen;
    PK11SlotListElement *slotElement, *tmpSlot;
    PRUint32 numSlots = 0, i = 0;
    PRBool canceled;
    nsresult rv = NS_OK;

    *aSlot = nsnull;

    // Get the slot
    slotList = PK11_GetAllTokens(MapGenMechToAlgoMech(aMechanism), 
                                PR_TRUE, PR_TRUE, m_ctx);
    if (!slotList || !slotList->head) {
        rv = NS_ERROR_FAILURE;
        goto loser;
    }

    if (!slotList->head->next) {
        /* only one slot available, just return it */
        *aSlot = slotList->head->slot;
      } else {
        // Gerenate a list of slots and ask the user to choose //
        tmpSlot = slotList->head;
        while (tmpSlot) {
            numSlots++;
            tmpSlot = tmpSlot->next;
        }

        // Allocate the slot name buffer //
        tokenNameList = NS_STATIC_CAST(PRUnichar**, nsMemory::Alloc(sizeof(PRUnichar *) * numSlots));
        i = 0;
        slotElement = PK11_GetFirstSafe(slotList);
        while (slotElement) {
			tokenNameList[i] = ToNewUnicode(NS_ConvertUTF8toUCS2(PK11_GetTokenName(slotElement->slot)));
            slotElement = PK11_GetNextSafe(slotList, slotElement, PR_FALSE);
            i++;
        }

		/* Throw up the token list dialog and get back the token */
		rv = getNSSDialogs((void**)&dialogs,
			               NS_GET_IID(nsITokenDialogs));

		if (NS_FAILED(rv)) goto loser;

		rv = dialogs->ChooseToken(nsnull, (const PRUnichar**)tokenNameList, numSlots, &unicodeTokenChosen, &canceled);
		NS_RELEASE(dialogs);
		if (NS_FAILED(rv)) goto loser;

		if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

        // Get the slot //
        slotElement = PK11_GetFirstSafe(slotList);
        nsAutoString tokenStr(unicodeTokenChosen);
        while (slotElement) {
            if (tokenStr.Equals(NS_ConvertUTF8toUCS2(PK11_GetTokenName(slotElement->slot)))) {
                *aSlot = slotElement->slot;
                break;
            }
            slotElement = PK11_GetNextSafe(slotList, slotElement, PR_FALSE);
        }
        if(!(*aSlot)) {
            rv = NS_ERROR_FAILURE;
            goto loser;
        }
      }

      // Get a reference to the slot //
      PK11_ReferenceSlot(*aSlot);
loser:
      if (slotList) {
          PK11_FreeSlotList(slotList);
      }
      if (tokenNameList) {
          nsMemory::Free(tokenNameList);
      }
      return rv;
}

nsresult
nsKeygenFormProcessor::GetPublicKey(nsString& aValue, nsString& aChallenge, 
				    nsString& aKeyType,
				    nsString& aOutPublicKey, nsString& aPqg)
{
    nsresult rv = NS_ERROR_FAILURE;
    char *keystring = nsnull;
    char *pqgString = nsnull, *str = nsnull;
    nsAutoString rsaStr;
    nsAutoString dsaStr;
    KeyType type;
    PRUint32 keyGenMechanism;
    PRInt32 primeBits;
    PQGParams *pqgParams;
    PK11SlotInfo *slot = nsnull;
    PK11RSAGenParams rsaParams;
    SECOidTag algTag;
    int keysize = 0;
    void *params;
    SECKEYPrivateKey *privateKey = nsnull;
    SECKEYPublicKey *publicKey = nsnull;
    CERTSubjectPublicKeyInfo *spkInfo = nsnull;
    PRArenaPool *arena = nsnull;
    SECStatus sec_rv = SECFailure;
    SECItem spkiItem;
    SECItem pkacItem;
    SECItem signedItem;
    CERTPublicKeyAndChallenge pkac;
    SECKeySizeChoiceInfo *choice = SECKeySizeChoiceList;
    nsIGeneratingKeypairInfoDialogs * dialogs;
    nsKeygenThread KeygenRunnable;
    nsCOMPtr<nsIKeygenThread> runnable;

    // Get the key size //
    while (choice) {
        if (aValue.Equals(choice->name)) {
            keysize = choice->size;
            break;
        }
        choice++;
    }
    if (!choice) {
        goto loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser;
    }

    // Set the keygen mechanism
    rsaStr.AssignWithConversion("rsa");
    dsaStr.AssignWithConversion("dsa");
    if (aKeyType.IsEmpty() || aKeyType.Equals(rsaStr)) {
        type = rsaKey;
        keyGenMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
    } else  if (aKeyType.Equals(dsaStr)) {
        char * end;
        pqgString = ToNewCString(aPqg);
        type = dsaKey;
        keyGenMechanism = CKM_DSA_KEY_PAIR_GEN;
        if (strcmp(pqgString, "null") == 0)
            goto loser;
            str = pqgString;
        do {
            end = strchr(str, ',');
            if (end != nsnull)
                *end = '\0';
            primeBits = pqg_prime_bits(str);
            if (choice->size == primeBits)
                goto found_match;
            str = end + 1;
        } while (end != nsnull);
        goto loser;
found_match:
        pqgParams = decode_pqg_params(str);
    } else {
        goto loser;
    }

    // Get the slot
    rv = GetSlot(keyGenMechanism, &slot);
    if (NS_FAILED(rv)) {
        goto loser;
    }
      switch (keyGenMechanism) {
        case CKM_RSA_PKCS_KEY_PAIR_GEN:
            rsaParams.keySizeInBits = keysize;
            rsaParams.pe = DEFAULT_RSA_KEYGEN_PE;
            algTag = DEFAULT_RSA_KEYGEN_ALG;
            params = &rsaParams;
            break;
        case CKM_DSA_KEY_PAIR_GEN:
            // XXX Fix this! XXX //
            goto loser;
      default:
          goto loser;
      }

    /* Make sure token is initialized. */
    rv = setPassword(slot, m_ctx);
    if (NS_FAILED(rv))
    goto loser;

    sec_rv = PK11_Authenticate(slot, PR_TRUE, m_ctx);
    if (sec_rv != SECSuccess) {
        goto loser;
    }

    rv = getNSSDialogs((void**)&dialogs,
                       NS_GET_IID(nsIGeneratingKeypairInfoDialogs));

    if (NS_FAILED(rv)) {
        privateKey = PK11_GenerateKeyPair(slot, keyGenMechanism, params,
                                          &publicKey, PR_TRUE, PR_TRUE, nsnull);
    } else {
        GenerateKeypairParameters gkp;
        gkp.privateKey = nsnull;
        gkp.publicKey = nsnull;
        gkp.slot = slot;
        gkp.keyGenMechanism = keyGenMechanism;
        gkp.params = params;
        KeygenRunnable.SetParams(&gkp);
        // Our parameters instance will be modified by the thread.

        runnable = do_QueryInterface(&KeygenRunnable);
        
        if (runnable) {
            rv = dialogs->DisplayGeneratingKeypairInfo(m_ctx, runnable);

            // We call join on the thread,
            // so we can be sure that no simultaneous access will happen.
            KeygenRunnable.Join();

            NS_RELEASE(dialogs);
            if (!NS_FAILED(rv)) {
                privateKey = gkp.privateKey;
                publicKey = gkp.publicKey;
                slot = gkp.slot;
                keyGenMechanism = gkp.keyGenMechanism;
                params = gkp.params;
            }
        }
    }
    
    if (!privateKey) {
        goto loser;
    }
    // just in case we'll need to authenticate to the db -jp //
    privateKey->wincx = m_ctx;

    /*
     * Create a subject public key info from the public key.
     */
    spkInfo = SECKEY_CreateSubjectPublicKeyInfo(publicKey);
    if ( !spkInfo ) {
        goto loser;
    }
    
    /*
     * Now DER encode the whole subjectPublicKeyInfo.
     */
    sec_rv=DER_Encode(arena, &spkiItem, CERTSubjectPublicKeyInfoTemplate, spkInfo);
    if (sec_rv != SECSuccess) {
        goto loser;
    }

    /*
     * set up the PublicKeyAndChallenge data structure, then DER encode it
     */
    pkac.spki = spkiItem;
	pkac.challenge.len = aChallenge.Length();
    pkac.challenge.data = (unsigned char *)ToNewCString(aChallenge);
    
    sec_rv = DER_Encode(arena, &pkacItem, CERTPublicKeyAndChallengeTemplate, &pkac);
    if ( sec_rv != SECSuccess ) {
        goto loser;
    }

    /*
     * now sign the DER encoded PublicKeyAndChallenge
     */
    sec_rv = SEC_DerSignData(arena, &signedItem, pkacItem.data, pkacItem.len,
			 privateKey, algTag);
    if ( sec_rv != SECSuccess ) {
        goto loser;
    }
    
    /*
     * Convert the signed public key and challenge into base64/ascii.
     */
    keystring = BTOA_DataToAscii(signedItem.data, signedItem.len);

    aOutPublicKey.AssignWithConversion(keystring);
    nsCRT::free(keystring);

    rv = NS_OK;
loser:
    if ( sec_rv != SECSuccess ) {
        if ( privateKey ) {
            PK11_DestroyTokenObject(privateKey->pkcs11Slot,privateKey->pkcs11ID);
            SECKEY_DestroyPrivateKey(privateKey);
        }
        if ( publicKey ) {
            PK11_DestroyTokenObject(publicKey->pkcs11Slot,publicKey->pkcs11ID);
        }
    }
    if ( spkInfo ) {
      SECKEY_DestroySubjectPublicKeyInfo(spkInfo);
    }
    if ( publicKey ) {
        SECKEY_DestroyPublicKey(publicKey);
    }
    if ( arena ) {
      PORT_FreeArena(arena, PR_TRUE);
    }
    if (slot != nsnull) {
        PK11_FreeSlot(slot);
    }
    return rv;
}

NS_METHOD 
nsKeygenFormProcessor::ProcessValue(nsIDOMHTMLElement *aElement, 
				    const nsString& aName, 
				    nsString& aValue) 
{ 
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLSelectElement>selectElement;
  nsresult res = aElement->QueryInterface(kIDOMHTMLSelectElementIID, 
					  getter_AddRefs(selectElement));
  if (NS_SUCCEEDED(res)) {
    nsAutoString keygenvalue;
    nsAutoString challengeValue;
    nsAutoString keyTypeValue;
    nsAutoString pqgValue;
    nsString publicKey;

    res = selectElement->GetAttribute(NS_LITERAL_STRING("_moz-type"), keygenvalue);
    if (NS_CONTENT_ATTR_HAS_VALUE == res && keygenvalue.Equals(NS_LITERAL_STRING("-mozilla-keygen"))) {

      res = selectElement->GetAttribute(NS_LITERAL_STRING("pqg"), pqgValue);
      res = selectElement->GetAttribute(NS_LITERAL_STRING("keytype"), keyTypeValue);
      if (NS_FAILED(res) || keyTypeValue.IsEmpty()) {
        // If this field is not present, we default to rsa.
  	    keyTypeValue.AssignWithConversion("rsa");
      }
      res = selectElement->GetAttribute(NS_LITERAL_STRING("challenge"), challengeValue);
      rv = GetPublicKey(aValue, challengeValue, keyTypeValue, 
			publicKey, pqgValue);
      aValue = publicKey;
    }
  }

  return rv; 
} 

NS_METHOD nsKeygenFormProcessor::ProvideContent(const nsString& aFormType, 
						nsVoidArray& aContent, 
						nsString& aAttribute) 
{ 
  nsString selectKey;
  SECKeySizeChoiceInfo *choice = SECKeySizeChoiceList;

  selectKey.AssignWithConversion("SELECT");
  if (Compare(aFormType, NS_LITERAL_STRING("SELECT"), 
    nsCaseInsensitiveStringComparator()) == 0) {
    for (SECKeySizeChoiceInfo* choice = SECKeySizeChoiceList; choice && choice->name; ++choice) {
      nsString *str = new nsString(choice->name);
      aContent.AppendElement(str);
    }
    aAttribute.AssignWithConversion(mozKeyGen);
  }
  return NS_OK;
} 



