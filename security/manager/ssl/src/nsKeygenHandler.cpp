/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secdert.h"
#include "nspr.h"
#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "keyhi.h"
#include "secder.h"
#include "cryptohi.h"
#include "base64.h"
#include "secasn1.h"
#include "pk11pqg.h"
#include "nsKeygenHandler.h"
#include "nsVoidArray.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIContent.h"
#include "nsKeygenThread.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsITokenDialogs.h"
#include "nsIGenKeypairInfoDlg.h"
#include "nsNSSShutDown.h"

//These defines are taken from the PKCS#11 spec
#define CKM_RSA_PKCS_KEY_PAIR_GEN     0x00000000
#define CKM_DH_PKCS_KEY_PAIR_GEN      0x00000020
#define CKM_DSA_KEY_PAIR_GEN          0x00000010

DERTemplate SECAlgorithmIDTemplate[] = {
    { DER_SEQUENCE,
          0, nullptr, sizeof(SECAlgorithmID) },
    { DER_OBJECT_ID,
          offsetof(SECAlgorithmID,algorithm), },
    { DER_OPTIONAL | DER_ANY,
          offsetof(SECAlgorithmID,parameters), },
    { 0, }
};

DERTemplate CERTSubjectPublicKeyInfoTemplate[] = {
    { DER_SEQUENCE,
          0, nullptr, sizeof(CERTSubjectPublicKeyInfo) },
    { DER_INLINE,
          offsetof(CERTSubjectPublicKeyInfo,algorithm),
          SECAlgorithmIDTemplate, },
    { DER_BIT_STRING,
          offsetof(CERTSubjectPublicKeyInfo,subjectPublicKey), },
    { 0, }
};

DERTemplate CERTPublicKeyAndChallengeTemplate[] =
{
    { DER_SEQUENCE, 0, nullptr, sizeof(CERTPublicKeyAndChallenge) },
    { DER_ANY, offsetof(CERTPublicKeyAndChallenge,spki), },
    { DER_IA5_STRING, offsetof(CERTPublicKeyAndChallenge,challenge), },
    { 0, }
};

const SEC_ASN1Template SECKEY_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(PQGParams) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(PQGParams,base) },
    { 0, }
};


static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

static PQGParams *
decode_pqg_params(char *aStr)
{
    unsigned char *buf = nullptr;
    unsigned int len;
    PLArenaPool *arena = nullptr;
    PQGParams *params = nullptr;
    SECStatus status;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
        return nullptr;

    params = static_cast<PQGParams*>(PORT_ArenaZAlloc(arena, sizeof(PQGParams)));
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
      PORT_FreeArena(arena, false);
    }
    if (buf) {
      PR_Free(buf);
    }
    return nullptr;
}

static int
pqg_prime_bits(char *str)
{
    PQGParams *params = nullptr;
    int primeBits = 0, i;

    params = decode_pqg_params(str);
    if (!params)
        goto done; /* lose */

    for (i = 0; params->prime.data[i] == 0; i++)
        /* empty */;
    primeBits = (params->prime.len - i) * 8;

done:
    if (params)
        PK11_PQG_DestroyParams(params);
    return primeBits;
}

typedef struct curveNameTagPairStr {
    const char *curveName;
    SECOidTag curveOidTag;
} CurveNameTagPair;

static CurveNameTagPair nameTagPair[] =
{ 
  { "prime192v1", SEC_OID_ANSIX962_EC_PRIME192V1 },
  { "prime192v2", SEC_OID_ANSIX962_EC_PRIME192V2 },
  { "prime192v3", SEC_OID_ANSIX962_EC_PRIME192V3 },
  { "prime239v1", SEC_OID_ANSIX962_EC_PRIME239V1 },
  { "prime239v2", SEC_OID_ANSIX962_EC_PRIME239V2 },
  { "prime239v3", SEC_OID_ANSIX962_EC_PRIME239V3 },
  { "prime256v1", SEC_OID_ANSIX962_EC_PRIME256V1 },

  { "secp112r1", SEC_OID_SECG_EC_SECP112R1},
  { "secp112r2", SEC_OID_SECG_EC_SECP112R2},
  { "secp128r1", SEC_OID_SECG_EC_SECP128R1},
  { "secp128r2", SEC_OID_SECG_EC_SECP128R2},
  { "secp160k1", SEC_OID_SECG_EC_SECP160K1},
  { "secp160r1", SEC_OID_SECG_EC_SECP160R1},
  { "secp160r2", SEC_OID_SECG_EC_SECP160R2},
  { "secp192k1", SEC_OID_SECG_EC_SECP192K1},
  { "secp192r1", SEC_OID_ANSIX962_EC_PRIME192V1 },
  { "nistp192", SEC_OID_ANSIX962_EC_PRIME192V1 },
  { "secp224k1", SEC_OID_SECG_EC_SECP224K1},
  { "secp224r1", SEC_OID_SECG_EC_SECP224R1},
  { "nistp224", SEC_OID_SECG_EC_SECP224R1},
  { "secp256k1", SEC_OID_SECG_EC_SECP256K1},
  { "secp256r1", SEC_OID_ANSIX962_EC_PRIME256V1 },
  { "nistp256", SEC_OID_ANSIX962_EC_PRIME256V1 },
  { "secp384r1", SEC_OID_SECG_EC_SECP384R1},
  { "nistp384", SEC_OID_SECG_EC_SECP384R1},
  { "secp521r1", SEC_OID_SECG_EC_SECP521R1},
  { "nistp521", SEC_OID_SECG_EC_SECP521R1},

  { "c2pnb163v1", SEC_OID_ANSIX962_EC_C2PNB163V1 },
  { "c2pnb163v2", SEC_OID_ANSIX962_EC_C2PNB163V2 },
  { "c2pnb163v3", SEC_OID_ANSIX962_EC_C2PNB163V3 },
  { "c2pnb176v1", SEC_OID_ANSIX962_EC_C2PNB176V1 },
  { "c2tnb191v1", SEC_OID_ANSIX962_EC_C2TNB191V1 },
  { "c2tnb191v2", SEC_OID_ANSIX962_EC_C2TNB191V2 },
  { "c2tnb191v3", SEC_OID_ANSIX962_EC_C2TNB191V3 },
  { "c2onb191v4", SEC_OID_ANSIX962_EC_C2ONB191V4 },
  { "c2onb191v5", SEC_OID_ANSIX962_EC_C2ONB191V5 },
  { "c2pnb208w1", SEC_OID_ANSIX962_EC_C2PNB208W1 },
  { "c2tnb239v1", SEC_OID_ANSIX962_EC_C2TNB239V1 },
  { "c2tnb239v2", SEC_OID_ANSIX962_EC_C2TNB239V2 },
  { "c2tnb239v3", SEC_OID_ANSIX962_EC_C2TNB239V3 },
  { "c2onb239v4", SEC_OID_ANSIX962_EC_C2ONB239V4 },
  { "c2onb239v5", SEC_OID_ANSIX962_EC_C2ONB239V5 },
  { "c2pnb272w1", SEC_OID_ANSIX962_EC_C2PNB272W1 },
  { "c2pnb304w1", SEC_OID_ANSIX962_EC_C2PNB304W1 },
  { "c2tnb359v1", SEC_OID_ANSIX962_EC_C2TNB359V1 },
  { "c2pnb368w1", SEC_OID_ANSIX962_EC_C2PNB368W1 },
  { "c2tnb431r1", SEC_OID_ANSIX962_EC_C2TNB431R1 },

  { "sect113r1", SEC_OID_SECG_EC_SECT113R1},
  { "sect113r2", SEC_OID_SECG_EC_SECT113R2},
  { "sect131r1", SEC_OID_SECG_EC_SECT131R1},
  { "sect131r2", SEC_OID_SECG_EC_SECT131R2},
  { "sect163k1", SEC_OID_SECG_EC_SECT163K1},
  { "nistk163", SEC_OID_SECG_EC_SECT163K1},
  { "sect163r1", SEC_OID_SECG_EC_SECT163R1},
  { "sect163r2", SEC_OID_SECG_EC_SECT163R2},
  { "nistb163", SEC_OID_SECG_EC_SECT163R2},
  { "sect193r1", SEC_OID_SECG_EC_SECT193R1},
  { "sect193r2", SEC_OID_SECG_EC_SECT193R2},
  { "sect233k1", SEC_OID_SECG_EC_SECT233K1},
  { "nistk233", SEC_OID_SECG_EC_SECT233K1},
  { "sect233r1", SEC_OID_SECG_EC_SECT233R1},
  { "nistb233", SEC_OID_SECG_EC_SECT233R1},
  { "sect239k1", SEC_OID_SECG_EC_SECT239K1},
  { "sect283k1", SEC_OID_SECG_EC_SECT283K1},
  { "nistk283", SEC_OID_SECG_EC_SECT283K1},
  { "sect283r1", SEC_OID_SECG_EC_SECT283R1},
  { "nistb283", SEC_OID_SECG_EC_SECT283R1},
  { "sect409k1", SEC_OID_SECG_EC_SECT409K1},
  { "nistk409", SEC_OID_SECG_EC_SECT409K1},
  { "sect409r1", SEC_OID_SECG_EC_SECT409R1},
  { "nistb409", SEC_OID_SECG_EC_SECT409R1},
  { "sect571k1", SEC_OID_SECG_EC_SECT571K1},
  { "nistk571", SEC_OID_SECG_EC_SECT571K1},
  { "sect571r1", SEC_OID_SECG_EC_SECT571R1},
  { "nistb571", SEC_OID_SECG_EC_SECT571R1},

};

SECKEYECParams * 
decode_ec_params(const char *curve)
{
    SECKEYECParams *ecparams;
    SECOidData *oidData = nullptr;
    SECOidTag curveOidTag = SEC_OID_UNKNOWN; /* default */
    int i, numCurves;

    if (curve && *curve) {
        numCurves = sizeof(nameTagPair)/sizeof(CurveNameTagPair);
        for (i = 0; ((i < numCurves) && (curveOidTag == SEC_OID_UNKNOWN)); 
             i++) {
            if (PL_strcmp(curve, nameTagPair[i].curveName) == 0)
                curveOidTag = nameTagPair[i].curveOidTag;
        }
    }

    /* Return nullptr if curve name is not recognized */
    if ((curveOidTag == SEC_OID_UNKNOWN) || 
        (oidData = SECOID_FindOIDByTag(curveOidTag)) == nullptr) {
        return nullptr;
    }

    ecparams = SECITEM_AllocItem(nullptr, nullptr, (2 + oidData->oid.len));

    if (!ecparams)
      return nullptr;

    /* 
     * ecparams->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in 
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    ecparams->data[0] = SEC_ASN1_OBJECT_ID;
    ecparams->data[1] = oidData->oid.len;
    memcpy(ecparams->data + 2, oidData->oid.data, oidData->oid.len);

    return ecparams;
}

NS_IMPL_ISUPPORTS1(nsKeygenFormProcessor, nsIFormProcessor)

nsKeygenFormProcessor::nsKeygenFormProcessor()
{ 
   m_ctx = new PipUIContext();

} 

nsKeygenFormProcessor::~nsKeygenFormProcessor()
{
}

nsresult
nsKeygenFormProcessor::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
  nsresult rv;
  NS_ENSURE_NO_AGGREGATION(aOuter);
  nsKeygenFormProcessor* formProc = new nsKeygenFormProcessor();

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

  nsCOMPtr<nsINSSComponent> nssComponent;
  nssComponent = do_GetService(kNSSComponentCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Init possible key size choices.
  nssComponent->GetPIPNSSBundleString("HighGrade", mSECKeySizeChoiceList[0].name);
  mSECKeySizeChoiceList[0].size = 2048;

  nssComponent->GetPIPNSSBundleString("MediumGrade", mSECKeySizeChoiceList[1].name);
  mSECKeySizeChoiceList[1].size = 1024;

  return NS_OK;
}

nsresult
nsKeygenFormProcessor::GetSlot(uint32_t aMechanism, PK11SlotInfo** aSlot)
{
  return GetSlotWithMechanism(aMechanism,m_ctx,aSlot);
}


uint32_t MapGenMechToAlgoMech(uint32_t mechanism)
{
    uint32_t searchMech;

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
    case CKM_EC_KEY_PAIR_GEN:
        /* The default should also work for EC key pair generation. */
    default:
        searchMech = mechanism;
        break;
    }
    return searchMech;
}


nsresult
GetSlotWithMechanism(uint32_t aMechanism, 
                     nsIInterfaceRequestor *m_ctx,
                     PK11SlotInfo** aSlot)
{
    nsNSSShutDownPreventionLock locker;
    PK11SlotList * slotList = nullptr;
    PRUnichar** tokenNameList = nullptr;
    nsITokenDialogs * dialogs;
    PRUnichar *unicodeTokenChosen;
    PK11SlotListElement *slotElement, *tmpSlot;
    uint32_t numSlots = 0, i = 0;
    bool canceled;
    nsresult rv = NS_OK;

    *aSlot = nullptr;

    // Get the slot
    slotList = PK11_GetAllTokens(MapGenMechToAlgoMech(aMechanism), 
                                true, true, m_ctx);
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
        tokenNameList = static_cast<PRUnichar**>(nsMemory::Alloc(sizeof(PRUnichar *) * numSlots));
        if (!tokenNameList) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto loser;
        }

        i = 0;
        slotElement = PK11_GetFirstSafe(slotList);
        while (slotElement) {
            tokenNameList[i] = UTF8ToNewUnicode(nsDependentCString(PK11_GetTokenName(slotElement->slot)));
            slotElement = PK11_GetNextSafe(slotList, slotElement, false);
            if (tokenNameList[i])
                i++;
            else {
                // OOM. adjust numSlots so we don't free unallocated memory. 
                numSlots = i;
                PK11_FreeSlotListElement(slotList, slotElement);
                rv = NS_ERROR_OUT_OF_MEMORY;
                goto loser;
            }
        }

		/* Throw up the token list dialog and get back the token */
		rv = getNSSDialogs((void**)&dialogs,
			               NS_GET_IID(nsITokenDialogs),
                     NS_TOKENDIALOGS_CONTRACTID);

		if (NS_FAILED(rv)) goto loser;

    {
      nsPSMUITracker tracker;
      if (!tokenNameList || !*tokenNameList) {
          rv = NS_ERROR_OUT_OF_MEMORY;
      }
      else if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        rv = dialogs->ChooseToken(m_ctx, (const PRUnichar**)tokenNameList, numSlots, &unicodeTokenChosen, &canceled);
      }
    }
		NS_RELEASE(dialogs);
		if (NS_FAILED(rv)) goto loser;

		if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

        // Get the slot //
        slotElement = PK11_GetFirstSafe(slotList);
        nsAutoString tokenStr(unicodeTokenChosen);
        while (slotElement) {
            if (tokenStr.Equals(NS_ConvertUTF8toUTF16(PK11_GetTokenName(slotElement->slot)))) {
                *aSlot = slotElement->slot;
                PK11_FreeSlotListElement(slotList, slotElement);
                break;
            }
            slotElement = PK11_GetNextSafe(slotList, slotElement, false);
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
          NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(numSlots, tokenNameList);
      }
      return rv;
}

nsresult
nsKeygenFormProcessor::GetPublicKey(nsAString& aValue, nsAString& aChallenge, 
				    nsAFlatString& aKeyType,
				    nsAString& aOutPublicKey, nsAString& aKeyParams)
{
    nsNSSShutDownPreventionLock locker;
    nsresult rv = NS_ERROR_FAILURE;
    char *keystring = nullptr;
    char *keyparamsString = nullptr, *str = nullptr;
    uint32_t keyGenMechanism;
    int32_t primeBits;
    PK11SlotInfo *slot = nullptr;
    PK11RSAGenParams rsaParams;
    SECOidTag algTag;
    int keysize = 0;
    void *params;
    SECKEYPrivateKey *privateKey = nullptr;
    SECKEYPublicKey *publicKey = nullptr;
    CERTSubjectPublicKeyInfo *spkInfo = nullptr;
    PLArenaPool *arena = nullptr;
    SECStatus sec_rv = SECFailure;
    SECItem spkiItem;
    SECItem pkacItem;
    SECItem signedItem;
    CERTPublicKeyAndChallenge pkac;
    pkac.challenge.data = nullptr;
    nsIGeneratingKeypairInfoDialogs * dialogs;
    nsKeygenThread *KeygenRunnable = 0;
    nsCOMPtr<nsIKeygenThread> runnable;
    
    // permanent and sensitive flags for keygen
    PK11AttrFlags attrFlags = PK11_ATTR_TOKEN | PK11_ATTR_SENSITIVE | PK11_ATTR_PRIVATE;

    // Get the key size //
    for (size_t i = 0; i < number_of_key_size_choices; ++i) {
        if (aValue.Equals(mSECKeySizeChoiceList[i].name)) {
            keysize = mSECKeySizeChoiceList[i].size;
            break;
        }
    }
    if (!keysize) {
        goto loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        goto loser;
    }

    // Set the keygen mechanism
    if (aKeyType.IsEmpty() || aKeyType.LowerCaseEqualsLiteral("rsa")) {
        keyGenMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
    } else if (aKeyType.LowerCaseEqualsLiteral("dsa")) {
        char * end;
        keyparamsString = ToNewCString(aKeyParams);
        if (!keyparamsString) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto loser;
        }

        keyGenMechanism = CKM_DSA_KEY_PAIR_GEN;
        if (strcmp(keyparamsString, "null") == 0)
            goto loser;
        str = keyparamsString;
        bool found_match = false;
        do {
            end = strchr(str, ',');
            if (end)
                *end = '\0';
            primeBits = pqg_prime_bits(str);
            if (keysize == primeBits) {
                found_match = true;
                break;
            }
            str = end + 1;
        } while (end);
        if (!found_match) {
            goto loser;
        }
    } else if (aKeyType.LowerCaseEqualsLiteral("ec")) {
        keyparamsString = ToNewCString(aKeyParams);
        if (!keyparamsString) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto loser;
        }

        keyGenMechanism = CKM_EC_KEY_PAIR_GEN;
        /* ecParams are initialized later */
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
        case CKM_EC_KEY_PAIR_GEN:
            /* XXX We ought to rethink how the KEYGEN tag is 
             * displayed. The pulldown selections presented
             * to the user must depend on the keytype.
             * The displayed selection could be picked
             * from the keyparams attribute (this is currently called
             * the pqg attribute).
             * For now, we pick ecparams from the keyparams field
             * if it specifies a valid supported curve, or else 
             * we pick one of secp384r1, secp256r1 or secp192r1
             * respectively depending on the user's selection
             * (High, Medium, Low). 
             * (RSA uses RSA-2048, RSA-1024 and RSA-512 for historical
             * reasons, while ECC choices represent a stronger mapping)
             * NOTE: The user's selection
             * is silently ignored when a valid curve is presented
             * in keyparams.
             */
            if ((params = decode_ec_params(keyparamsString)) == nullptr) {
                /* The keyparams attribute did not specify a valid
                 * curve name so use a curve based on the keysize.
                 * NOTE: Here keysize is used only as an indication of
                 * High/Medium/Low strength; elliptic curve
                 * cryptography uses smaller keys than RSA to provide
                 * equivalent security.
                 */
                switch (keysize) {
                case 2048:
                    params = decode_ec_params("secp384r1");
                    break;
                case 1024:
                case 512:
                    params = decode_ec_params("secp256r1");
                    break;
                } 
            }
            /* XXX The signature algorithm ought to choose the hashing
             * algorithm based on key size once ECDSA variations based
             * on SHA256 SHA384 and SHA512 are standardized.
             */
            algTag = SEC_OID_ANSIX962_ECDSA_SIGNATURE_WITH_SHA1_DIGEST;
            break;
      default:
          goto loser;
      }

    /* Make sure token is initialized. */
    rv = setPassword(slot, m_ctx);
    if (NS_FAILED(rv))
        goto loser;

    sec_rv = PK11_Authenticate(slot, true, m_ctx);
    if (sec_rv != SECSuccess) {
        goto loser;
    }

    rv = getNSSDialogs((void**)&dialogs,
                       NS_GET_IID(nsIGeneratingKeypairInfoDialogs),
                       NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID);

    if (NS_SUCCEEDED(rv)) {
        KeygenRunnable = new nsKeygenThread();
        NS_IF_ADDREF(KeygenRunnable);
    }

    if (NS_FAILED(rv) || !KeygenRunnable) {
        rv = NS_OK;
        privateKey = PK11_GenerateKeyPairWithFlags(slot, keyGenMechanism, params,
                                                   &publicKey, attrFlags, m_ctx);
    } else {
        KeygenRunnable->SetParams( slot, attrFlags, nullptr, 0,
                                   keyGenMechanism, params, m_ctx );

        runnable = do_QueryInterface(KeygenRunnable);
        
        if (runnable) {
            {
              nsPSMUITracker tracker;
              if (tracker.isUIForbidden()) {
                rv = NS_ERROR_NOT_AVAILABLE;
              }
              else {
                rv = dialogs->DisplayGeneratingKeypairInfo(m_ctx, runnable);
                // We call join on the thread, 
                // so we can be sure that no simultaneous access to the passed parameters will happen.
                KeygenRunnable->Join();
              }
            }

            NS_RELEASE(dialogs);
            if (NS_SUCCEEDED(rv)) {
                PK11SlotInfo *used_slot = nullptr;
                rv = KeygenRunnable->ConsumeResult(&used_slot, &privateKey, &publicKey);
                if (NS_SUCCEEDED(rv) && used_slot) {
                  PK11_FreeSlot(used_slot);
                }
            }
        }
    }
    
    if (NS_FAILED(rv) || !privateKey) {
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
    if (!pkac.challenge.data) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto loser;
    }
    
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
    if (!keystring) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto loser;
    }

    CopyASCIItoUTF16(keystring, aOutPublicKey);
    free(keystring);

    rv = NS_OK;
loser:
    if ( sec_rv != SECSuccess ) {
        if ( privateKey ) {
            PK11_DestroyTokenObject(privateKey->pkcs11Slot,privateKey->pkcs11ID);
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
    if ( privateKey ) {
        SECKEY_DestroyPrivateKey(privateKey);
    }
    if ( arena ) {
        PORT_FreeArena(arena, true);
    }
    if (slot) {
        PK11_FreeSlot(slot);
    }
    if (KeygenRunnable) {
        NS_RELEASE(KeygenRunnable);
    }
    if (keyparamsString) {
        nsMemory::Free(keyparamsString);
    }
    if (pkac.challenge.data) {
        nsMemory::Free(pkac.challenge.data);
    }
    return rv;
}

NS_METHOD 
nsKeygenFormProcessor::ProcessValue(nsIDOMHTMLElement *aElement, 
				    const nsAString& aName, 
				    nsAString& aValue) 
{ 
    nsAutoString challengeValue;
    nsAutoString keyTypeValue;
    nsAutoString keyParamsValue;
    
    aElement->GetAttribute(NS_LITERAL_STRING("keytype"), keyTypeValue);
    if (keyTypeValue.IsEmpty()) {
        // If this field is not present, we default to rsa.
        keyTypeValue.AssignLiteral("rsa");
    }
    
    aElement->GetAttribute(NS_LITERAL_STRING("pqg"), 
                           keyParamsValue);
    /* XXX We can still support the pqg attribute in the keygen 
     * tag for backward compatibility while introducing a more 
     * general attribute named keyparams.
     */
    if (keyParamsValue.IsEmpty()) {
        aElement->GetAttribute(NS_LITERAL_STRING("keyparams"), 
                               keyParamsValue);
    }

    aElement->GetAttribute(NS_LITERAL_STRING("challenge"), challengeValue);

    return GetPublicKey(aValue, challengeValue, keyTypeValue, 
                        aValue, keyParamsValue);
} 

NS_METHOD nsKeygenFormProcessor::ProvideContent(const nsAString& aFormType, 
						nsTArray<nsString>& aContent, 
						nsAString& aAttribute) 
{ 
  if (Compare(aFormType, NS_LITERAL_STRING("SELECT"), 
    nsCaseInsensitiveStringComparator()) == 0) {

    for (size_t i = 0; i < number_of_key_size_choices; ++i) {
      aContent.AppendElement(mSECKeySizeChoiceList[i].name);
    }
    aAttribute.AssignLiteral("-mozilla-keygen");
  }
  return NS_OK;
} 

