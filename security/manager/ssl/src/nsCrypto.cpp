/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCrypto.h"
#include "nsNSSComponent.h"
#include "secmod.h"

#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsISaveAsCharset.h"
#include "nsNativeCharsetUtils.h"
#include "nsServiceManagerUtils.h"

#ifndef MOZ_DISABLE_CRYPTOLEGACY
#include "nsKeygenHandler.h"
#include "nsKeygenThread.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsThreadUtils.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsAlgorithm.h"
#include "prprf.h"
#include "nsDOMCID.h"
#include "nsIDOMWindow.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"
#include "nsIXPConnect.h"
#include "nsIRunnable.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIFilePicker.h"
#include "nsJSPrincipals.h"
#include "nsJSUtils.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIGenKeypairInfoDlg.h"
#include "nsIDOMCryptoDialogs.h"
#include "nsIFormSigningDialog.h"
#include "nsIContentSecurityPolicy.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include <ctype.h>
#include "pk11func.h"
#include "keyhi.h"
#include "cryptohi.h"
#include "seccomon.h"
#include "secerr.h"
#include "sechash.h"
#include "crmf.h"
#include "pk11pqg.h"
#include "cmmf.h"
#include "nssb64.h"
#include "base64.h"
#include "cert.h"
#include "certdb.h"
#include "secmod.h"
#include "ScopedNSSTypes.h"

#include "ssl.h" // For SSL_ClearSessionCache

#include "nsNSSCleaner.h"

#include "nsNSSCertHelper.h"
#include <algorithm>
#include "nsWrapperCacheInlines.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

/*
 * These are the most common error strings that are returned
 * by the JavaScript methods in case of error.
 */

#define JS_ERROR       "error:"
#define JS_ERROR_INTERNAL  JS_ERROR"internalError"

#undef REPORT_INCORRECT_NUM_ARGS

#define JS_OK_ADD_MOD                      3
#define JS_OK_DEL_EXTERNAL_MOD             2
#define JS_OK_DEL_INTERNAL_MOD             1

#define JS_ERR_INTERNAL                   -1
#define JS_ERR_USER_CANCEL_ACTION         -2
#define JS_ERR_INCORRECT_NUM_OF_ARGUMENTS -3
#define JS_ERR_DEL_MOD                    -4
#define JS_ERR_ADD_MOD                    -5
#define JS_ERR_BAD_MODULE_NAME            -6
#define JS_ERR_BAD_DLL_NAME               -7
#define JS_ERR_BAD_MECHANISM_FLAGS        -8
#define JS_ERR_BAD_CIPHER_ENABLE_FLAGS    -9
#define JS_ERR_ADD_DUPLICATE_MOD          -10

namespace {
  
NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

} // unnamed namespace

#ifndef MOZ_DISABLE_CRYPTOLEGACY

NSSCleanupAutoPtrClass_WithParam(PK11Context, PK11_DestroyContext, TrueParam, true)

/*
 * This structure is used to store information for one key generation.
 * The nsCrypto::GenerateCRMFRequest method parses the inputs and then
 * stores one of these structures for every key generation that happens.
 * The information stored in this structure is then used to set some
 * values in the CRMF request.
 */
typedef enum {
  rsaEnc, rsaDualUse, rsaSign, rsaNonrepudiation, rsaSignNonrepudiation,
  ecEnc, ecDualUse, ecSign, ecNonrepudiation, ecSignNonrepudiation,
  dhEx, dsaSignNonrepudiation, dsaSign, dsaNonrepudiation, invalidKeyGen
} nsKeyGenType;

bool isECKeyGenType(nsKeyGenType kgt)
{
  switch (kgt)
  {
    case ecEnc:
    case ecDualUse:
    case ecSign:
    case ecNonrepudiation:
    case ecSignNonrepudiation:
      return true;
    
    default:
      break;
  }

  return false;
}

typedef struct nsKeyPairInfoStr {
  SECKEYPublicKey  *pubKey;     /* The putlic key associated with gen'd 
                                   priv key. */
  SECKEYPrivateKey *privKey;    /* The private key we generated */ 
  nsKeyGenType      keyGenType; /* What type of key gen are we doing.*/

  CERTCertificate *ecPopCert;
                   /* null: use signing for pop
                      other than null: a cert that defines EC keygen params
                                       and will be used for dhMac PoP. */

  SECKEYPublicKey  *ecPopPubKey;
                   /* extracted public key from ecPopCert */
} nsKeyPairInfo;


//This class is just used to pass arguments
//to the nsCryptoRunnable event.
class nsCryptoRunArgs : public nsISupports {
public:
  nsCryptoRunArgs();
  virtual ~nsCryptoRunArgs();
  nsCOMPtr<nsISupports> m_kungFuDeathGrip;
  JSContext *m_cx;
  JSObject  *m_scope;
  nsCOMPtr<nsIPrincipal> m_principals;
  nsXPIDLCString m_jsCallback;
  NS_DECL_ISUPPORTS
};

//This class is used to run the callback code
//passed to crypto.generateCRMFRequest
//We have to do that for backwards compatibility
//reasons w/ PSM 1.x and Communciator 4.x
class nsCryptoRunnable : public nsIRunnable {
public:
  nsCryptoRunnable(nsCryptoRunArgs *args);
  virtual ~nsCryptoRunnable();

  NS_IMETHOD Run ();
  NS_DECL_ISUPPORTS
private:
  nsCryptoRunArgs *m_args;
};


//We're going to inherit the memory passed
//into us.
//This class backs up an array of certificates
//as an event.
class nsP12Runnable : public nsIRunnable {
public:
  nsP12Runnable(nsIX509Cert **certArr, int32_t numCerts, nsIPK11Token *token);
  virtual ~nsP12Runnable();

  NS_IMETHOD Run();
  NS_DECL_ISUPPORTS
private:
  nsCOMPtr<nsIPK11Token> mToken;
  nsIX509Cert **mCertArr;
  int32_t       mNumCerts;
};

// QueryInterface implementation for nsCrypto
NS_INTERFACE_MAP_BEGIN(nsCrypto)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
NS_INTERFACE_MAP_END_INHERITING(mozilla::dom::Crypto)

NS_IMPL_ADDREF_INHERITED(nsCrypto, mozilla::dom::Crypto)
NS_IMPL_RELEASE_INHERITED(nsCrypto, mozilla::dom::Crypto)
 
// QueryInterface implementation for nsCRMFObject
NS_INTERFACE_MAP_BEGIN(nsCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CRMFObject)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCRMFObject)
NS_IMPL_RELEASE(nsCRMFObject)

// QueryInterface implementation for nsPkcs11
#endif // MOZ_DISABLE_CRYPTOLEGACY

NS_INTERFACE_MAP_BEGIN(nsPkcs11)
  NS_INTERFACE_MAP_ENTRY(nsIPKCS11)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPkcs11)
NS_IMPL_RELEASE(nsPkcs11)

#ifndef MOZ_DISABLE_CRYPTOLEGACY

// ISupports implementation for nsCryptoRunnable
NS_IMPL_ISUPPORTS1(nsCryptoRunnable, nsIRunnable)

// ISupports implementation for nsP12Runnable
NS_IMPL_ISUPPORTS1(nsP12Runnable, nsIRunnable)

// ISupports implementation for nsCryptoRunArgs
NS_IMPL_ISUPPORTS0(nsCryptoRunArgs)

nsCrypto::nsCrypto() :
  mEnableSmartCardEvents(false)
{
}

nsCrypto::~nsCrypto()
{
}

void
nsCrypto::Init(nsIDOMWindow* aWindow)
{
  mozilla::dom::Crypto::Init(aWindow);
}

void
nsCrypto::SetEnableSmartCardEvents(bool aEnable, ErrorResult& aRv)
{
  nsresult rv = NS_OK;

  // this has the side effect of starting the nssComponent (and initializing
  // NSS) even if it isn't already going. Starting the nssComponent is a
  // prerequisite for getting smartCard events.
  if (aEnable) {
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  mEnableSmartCardEvents = aEnable;
}

NS_IMETHODIMP
nsCrypto::SetEnableSmartCardEvents(bool aEnable)
{
  ErrorResult rv;
  SetEnableSmartCardEvents(aEnable, rv);
  return rv.ErrorCode();
}

bool
nsCrypto::EnableSmartCardEvents()
{
  return mEnableSmartCardEvents;
}

NS_IMETHODIMP
nsCrypto::GetEnableSmartCardEvents(bool *aEnable)
{
  *aEnable = EnableSmartCardEvents();
  return NS_OK;
}

//A quick function to let us know if the key we're trying to generate
//can be escrowed.
static bool
ns_can_escrow(nsKeyGenType keyGenType)
{
  /* For now, we only escrow rsa-encryption and ec-encryption keys. */
  return (bool)(keyGenType == rsaEnc || keyGenType == ecEnc);
}

//Retrieve crypto.version so that callers know what
//version of PSM this is.
void
nsCrypto::GetVersion(nsString& aVersion)
{
  aVersion.Assign(NS_LITERAL_STRING(PSM_VERSION_STRING));
}

/*
 * Given an nsKeyGenType, return the PKCS11 mechanism that will
 * perform the correct key generation.
 */
static uint32_t
cryptojs_convert_to_mechanism(nsKeyGenType keyGenType)
{
  uint32_t retMech;

  switch (keyGenType) {
  case rsaEnc:
  case rsaDualUse:
  case rsaSign:
  case rsaNonrepudiation:
  case rsaSignNonrepudiation:
    retMech = CKM_RSA_PKCS_KEY_PAIR_GEN;
    break;
  case ecEnc:
  case ecDualUse:
  case ecSign:
  case ecNonrepudiation:
  case ecSignNonrepudiation:
    retMech = CKM_EC_KEY_PAIR_GEN;
    break;
  case dhEx:
    retMech = CKM_DH_PKCS_KEY_PAIR_GEN;
    break;
  case dsaSign:
  case dsaSignNonrepudiation:
  case dsaNonrepudiation:
    retMech = CKM_DSA_KEY_PAIR_GEN;
    break;
  default:
    retMech = CKM_INVALID_MECHANISM;
  }
  return retMech;
}

/*
 * This function takes a string read through JavaScript parameters
 * and translates it to the internal enumeration representing the
 * key gen type. Leading and trailing whitespace must be already removed.
 */
static nsKeyGenType
cryptojs_interpret_key_gen_type(const nsAString& keyAlg)
{
  if (keyAlg.EqualsLiteral("rsa-ex")) {
    return rsaEnc;
  }
  if (keyAlg.EqualsLiteral("rsa-dual-use")) {
    return rsaDualUse;
  }
  if (keyAlg.EqualsLiteral("rsa-sign")) {
    return rsaSign;
  }
  if (keyAlg.EqualsLiteral("rsa-sign-nonrepudiation")) {
    return rsaSignNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("rsa-nonrepudiation")) {
    return rsaNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("ec-ex")) {
    return ecEnc;
  }
  if (keyAlg.EqualsLiteral("ec-dual-use")) {
    return ecDualUse;
  }
  if (keyAlg.EqualsLiteral("ec-sign")) {
    return ecSign;
  }
  if (keyAlg.EqualsLiteral("ec-sign-nonrepudiation")) {
    return ecSignNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("ec-nonrepudiation")) {
    return ecNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("dsa-sign-nonrepudiation")) {
    return dsaSignNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("dsa-sign")) {
    return dsaSign;
  }
  if (keyAlg.EqualsLiteral("dsa-nonrepudiation")) {
    return dsaNonrepudiation;
  }
  if (keyAlg.EqualsLiteral("dh-ex")) {
    return dhEx;
  }
  return invalidKeyGen;
}

/* 
 * input: null terminated char* pointing to (the remainder of) an
 * EC key param string.
 *
 * bool return value, false means "no more name=value pair found",
 *                    true means "found, see out params"
 * 
 * out param name: char * pointing to name (not zero terminated)
 * out param name_len: length of found name
 * out param value: char * pointing to value (not zero terminated)
 * out param value_len: length of found value
 * out param next_pair: to be used for a follow up call to this function
 */

bool getNextNameValueFromECKeygenParamString(char *input,
                                               char *&name,
                                               int &name_len,
                                               char *&value,
                                               int &value_len,
                                               char *&next_call)
{
  if (!input || !*input)
    return false;

  // we allow leading ; and leading space in front of each name value pair

  while (*input && *input == ';')
    ++input;

  while (*input && *input == ' ')
    ++input;

  name = input;

  while (*input && *input != '=')
    ++input;

  if (*input != '=')
    return false;

  name_len = input - name;
  ++input;

  value = input;

  while (*input && *input != ';')
    ++input;

  value_len = input - value;
  next_call = input;

  return true;
}

//Take the string passed into us via crypto.generateCRMFRequest
//as the keygen type parameter and convert it to parameters 
//we can actually pass to the PKCS#11 layer.
static void*
nsConvertToActualKeyGenParams(uint32_t keyGenMech, char *params,
                              uint32_t paramLen, int32_t keySize,
                              nsKeyPairInfo *keyPairInfo)
{
  void *returnParams = nullptr;


  switch (keyGenMech) {
  case CKM_RSA_PKCS_KEY_PAIR_GEN:
  {
    // For RSA, we don't support passing in key generation arguments from
    // the JS code just yet.
    if (params)
      return nullptr;

    PK11RSAGenParams *rsaParams;
    rsaParams = static_cast<PK11RSAGenParams*>
                           (nsMemory::Alloc(sizeof(PK11RSAGenParams)));
                              
    if (!rsaParams) {
      return nullptr;
    }
    /* I'm just taking the same parameters used in 
     * certdlgs.c:GenKey
     */
    if (keySize > 0) {
      rsaParams->keySizeInBits = keySize;
    } else {
      rsaParams->keySizeInBits = 1024;
    }
    rsaParams->pe = DEFAULT_RSA_KEYGEN_PE;
    returnParams = rsaParams;
    break;
  }
  case CKM_EC_KEY_PAIR_GEN:
  {
    /*
     * keygen params for generating EC keys must be composed of name=value pairs,
     * multiple pairs allowed, separated using semicolon ;
     *
     * Either param "curve" or param "popcert" must be specified.
     * curve=name-of-curve
     * popcert=base64-encoded-cert
     *
     * When both params are specified, popcert will be used.
     * If no popcert param is given, or if popcert can not be decoded,
     * we will fall back to the curve param.
     *
     * Additional name=value pairs may be defined in the future.
     *
     * If param popcert is present and valid, the given certificate will be used
     * to determine the key generation params. In addition the certificate
     * will be used to produce a dhMac based Proof of Posession,
     * using the cert's public key, subject and issuer names,
     * as specified in RFC 2511 section 4.3 paragraph 2 and Appendix A.
     *
     * If neither param popcert nor param curve could be used,
     * tse a curve based on the keysize param.
     * NOTE: Here keysize is used only as an indication of
     * High/Medium/Low strength; elliptic curve
     * cryptography uses smaller keys than RSA to provide
     * equivalent security.
     */

    char *curve = nullptr;

    {
      // extract components of name=value list

      char *next_input = params;
      char *name = nullptr;
      char *value = nullptr;
      int name_len = 0;
      int value_len = 0;
  
      while (getNextNameValueFromECKeygenParamString(
              next_input, name, name_len, value, value_len,
              next_input))
      {
        if (PL_strncmp(name, "curve", std::min(name_len, 5)) == 0)
        {
          curve = PL_strndup(value, value_len);
        }
        else if (PL_strncmp(name, "popcert", std::min(name_len, 7)) == 0)
        {
          char *certstr = PL_strndup(value, value_len);
          if (certstr) {
            keyPairInfo->ecPopCert = CERT_ConvertAndDecodeCertificate(certstr);
            PL_strfree(certstr);

            if (keyPairInfo->ecPopCert)
            {
              keyPairInfo->ecPopPubKey = CERT_ExtractPublicKey(keyPairInfo->ecPopCert);
            }
          }
        }
      }
    }

    // first try to use the params of the provided CA cert
    if (keyPairInfo->ecPopPubKey)
    {
      returnParams = SECITEM_DupItem(&keyPairInfo->ecPopPubKey->u.ec.DEREncodedParams);
    }

    // if we did not yet find good params, do we have a curve name?
    if (!returnParams && curve)
    {
      returnParams = decode_ec_params(curve);
    }

    // if we did not yet find good params, do something based on keysize
    if (!returnParams)
    {
      switch (keySize) {
      case 512:
      case 1024:
          returnParams = decode_ec_params("secp256r1");
          break;
      case 2048:
      default:
          returnParams = decode_ec_params("secp384r1");
          break;
      }
    }

    if (curve)
      PL_strfree(curve);

    break;
  }
  case CKM_DSA_KEY_PAIR_GEN:
  {
    // For DSA, we don't support passing in key generation arguments from
    // the JS code just yet.
    if (params)
      return nullptr;

    PQGParams *pqgParams = nullptr;
    PQGVerify *vfy = nullptr;
    SECStatus  rv;
    int        index;
       
    index = PQG_PBITS_TO_INDEX(keySize);
    if (index == -1) {
      returnParams = nullptr;
      break;
    }
    rv = PK11_PQG_ParamGen(0, &pqgParams, &vfy);
    if (vfy) {
      PK11_PQG_DestroyVerify(vfy);
    }
    if (rv != SECSuccess) {
      if (pqgParams) {
        PK11_PQG_DestroyParams(pqgParams);
      }
      return nullptr;
    }
    returnParams = pqgParams;
    break;
  }
  default:
    returnParams = nullptr;
  }
  return returnParams;
}

//We need to choose which PKCS11 slot we're going to generate
//the key on.  Calls the default implementation provided by
//nsKeygenHandler.cpp
static PK11SlotInfo*
nsGetSlotForKeyGen(nsKeyGenType keyGenType, nsIInterfaceRequestor *ctx)
{
  nsNSSShutDownPreventionLock locker;
  uint32_t mechanism = cryptojs_convert_to_mechanism(keyGenType);
  PK11SlotInfo *slot = nullptr;
  nsresult rv = GetSlotWithMechanism(mechanism,ctx, &slot);
  if (NS_FAILED(rv)) {
    if (slot)
      PK11_FreeSlot(slot);
    slot = nullptr;
  }
  return slot;
}

//Free the parameters that were passed into PK11_GenerateKeyPair
//depending on the mechanism type used.
static void
nsFreeKeyGenParams(CK_MECHANISM_TYPE keyGenMechanism, void *params)
{
  switch (keyGenMechanism) {
  case CKM_RSA_PKCS_KEY_PAIR_GEN:
    nsMemory::Free(params);
    break;
  case CKM_EC_KEY_PAIR_GEN:
    SECITEM_FreeItem(reinterpret_cast<SECItem*>(params), true);
    break;
  case CKM_DSA_KEY_PAIR_GEN:
    PK11_PQG_DestroyParams(static_cast<PQGParams*>(params));
    break;
  }
}

//Function that is used to generate a single key pair.
//Once all the arguments have been parsed and processed, this
//function gets called and takes care of actually generating
//the key pair passing the appopriate parameters to the NSS
//functions.
static nsresult
cryptojs_generateOneKeyPair(JSContext *cx, nsKeyPairInfo *keyPairInfo, 
                            int32_t keySize, char *params, 
                            nsIInterfaceRequestor *uiCxt,
                            PK11SlotInfo *slot, bool willEscrow)
                            
{
  const PK11AttrFlags sensitiveFlags = (PK11_ATTR_SENSITIVE | PK11_ATTR_PRIVATE);
  const PK11AttrFlags temporarySessionFlags = PK11_ATTR_SESSION;
  const PK11AttrFlags permanentTokenFlags = PK11_ATTR_TOKEN;
  const PK11AttrFlags extractableFlags = PK11_ATTR_EXTRACTABLE;
  
  nsIGeneratingKeypairInfoDialogs * dialogs;
  nsKeygenThread *KeygenRunnable = 0;
  nsCOMPtr<nsIKeygenThread> runnable;

  uint32_t mechanism = cryptojs_convert_to_mechanism(keyPairInfo->keyGenType);
  void *keyGenParams = nsConvertToActualKeyGenParams(mechanism, params, 
                                                     (params) ? strlen(params):0, 
                                                     keySize, keyPairInfo);

  if (!keyGenParams || !slot) {
    return NS_ERROR_INVALID_ARG;
  }

  // Make sure the token has password already set on it before trying
  // to generate the key.

  nsresult rv = setPassword(slot, uiCxt);
  if (NS_FAILED(rv))
    return rv;

  if (PK11_Authenticate(slot, true, uiCxt) != SECSuccess)
    return NS_ERROR_FAILURE;
 
  // Smart cards will not let you extract a private key once 
  // it is on the smart card.  If we've been told to escrow
  // a private key that will be stored on a smart card,
  // then we'll use the following strategy to ensure we can escrow it.
  // We'll attempt to generate the key on our internal token,
  // because this is expected to avoid some problems.
  // If it works, we'll escrow, move the key to the smartcard, done.
  // If it didn't work (or the internal key doesn't support the desired
  // mechanism), then we'll attempt to generate the key on
  // the destination token, with the EXTRACTABLE flag set.
  // If it works, we'll extract, escrow, done.
  // If it failed, then we're unable to escrow and return failure.
  // NOTE: We call PK11_GetInternalSlot instead of PK11_GetInternalKeySlot
  //       so that the key has zero chance of being store in the
  //       user's key3.db file.  Which the slot returned by
  //       PK11_GetInternalKeySlot has access to and PK11_GetInternalSlot
  //       does not.
  ScopedPK11SlotInfo intSlot;
  
  if (willEscrow && !PK11_IsInternal(slot)) {
    intSlot = PK11_GetInternalSlot();
    NS_ASSERTION(intSlot,"Couldn't get the internal slot");
    
    if (!PK11_DoesMechanism(intSlot, mechanism)) {
      // Set to null, and the subsequent code will not attempt to use it.
      intSlot = nullptr;
    }
  }

  rv = getNSSDialogs((void**)&dialogs,
                     NS_GET_IID(nsIGeneratingKeypairInfoDialogs),
                     NS_GENERATINGKEYPAIRINFODIALOGS_CONTRACTID);

  if (NS_SUCCEEDED(rv)) {
    KeygenRunnable = new nsKeygenThread();
    if (KeygenRunnable) {
      NS_ADDREF(KeygenRunnable);
    }
  }
  
  // "firstAttemptSlot" and "secondAttemptSlot" are alternative names
  // for better code readability, we don't increase the reference counts.
  
  PK11SlotInfo *firstAttemptSlot = nullptr;
  PK11AttrFlags firstAttemptFlags = 0;

  PK11SlotInfo *secondAttemptSlot = slot;
  PK11AttrFlags secondAttemptFlags = sensitiveFlags | permanentTokenFlags;
  
  if (willEscrow) {
    secondAttemptFlags |= extractableFlags;
  }
  
  if (!intSlot || PK11_IsInternal(slot)) {
    // if we cannot use the internal slot, then there is only one attempt
    // if the destination slot is the internal slot, then there is only one attempt
    firstAttemptSlot = secondAttemptSlot;
    firstAttemptFlags = secondAttemptFlags;
    secondAttemptSlot = nullptr;
    secondAttemptFlags = 0;
  }
  else {
    firstAttemptSlot = intSlot;
    firstAttemptFlags = sensitiveFlags | temporarySessionFlags;
    
    // We always need the extractable flag on the first attempt,
    // because we want to move the key to another slot - ### is this correct?
    firstAttemptFlags |= extractableFlags;
  }

  bool mustMoveKey = false;
  
  if (NS_FAILED(rv) || !KeygenRunnable) {
    /* execute key generation on this thread */
    rv = NS_OK;
    
    keyPairInfo->privKey = 
      PK11_GenerateKeyPairWithFlags(firstAttemptSlot, mechanism,
                                    keyGenParams, &keyPairInfo->pubKey,
                                    firstAttemptFlags, uiCxt);
    
    if (keyPairInfo->privKey) {
      // success on first attempt
      if (secondAttemptSlot) {
        mustMoveKey = true;
      }
    }
    else {
      keyPairInfo->privKey = 
        PK11_GenerateKeyPairWithFlags(secondAttemptSlot, mechanism,
                                      keyGenParams, &keyPairInfo->pubKey,
                                      secondAttemptFlags, uiCxt);
    }
    
  } else {
    /* execute key generation on separate thread */
    KeygenRunnable->SetParams( firstAttemptSlot, firstAttemptFlags,
                               secondAttemptSlot, secondAttemptFlags,
                               mechanism, keyGenParams, uiCxt );

    runnable = do_QueryInterface(KeygenRunnable);

    if (runnable) {
      {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        }
        else {
          rv = dialogs->DisplayGeneratingKeypairInfo(uiCxt, runnable);
          // We call join on the thread, 
          // so we can be sure that no simultaneous access to the passed parameters will happen.
          KeygenRunnable->Join();
        }
      }

      NS_RELEASE(dialogs);
      if (NS_SUCCEEDED(rv)) {
        PK11SlotInfo *used_slot = nullptr;
        rv = KeygenRunnable->ConsumeResult(&used_slot, 
                                           &keyPairInfo->privKey, &keyPairInfo->pubKey);

        if (NS_SUCCEEDED(rv)) {
          if ((used_slot == firstAttemptSlot) && secondAttemptSlot) {
            mustMoveKey = true;
          }
        
          if (used_slot) {
            PK11_FreeSlot(used_slot);
          }
        }
      }
    }
  }

  firstAttemptSlot = nullptr;
  secondAttemptSlot = nullptr;
  
  nsFreeKeyGenParams(mechanism, keyGenParams);

  if (KeygenRunnable) {
    NS_RELEASE(KeygenRunnable);
  }

  if (!keyPairInfo->privKey || !keyPairInfo->pubKey) {
    return NS_ERROR_FAILURE;
  }
 
  //If we generated the key pair on the internal slot because the
  // keys were going to be escrowed, move the keys over right now.
  if (mustMoveKey) {
    ScopedSECKEYPrivateKey newPrivKey(PK11_LoadPrivKey(slot,
                                                    keyPairInfo->privKey,
                                                    keyPairInfo->pubKey,
                                                    true, true));
    if (!newPrivKey)
      return NS_ERROR_FAILURE;

    // The private key is stored on the selected slot now, and the copy we
    // ultimately use for escrowing when the time comes lives 
    // in the internal slot.  We will delete it from that slot
    // after the requests are made.
  }  

  return NS_OK;
}

/*
 * FUNCTION: cryptojs_ReadArgsAndGenerateKey
 * -------------------------------------
 * INPUTS:
 *  cx
 *    The JSContext associated with the execution of the corresponging
 *    crypto.generateCRMFRequest call
 *  argv
 *    A pointer to an array of JavaScript parameters passed to the
 *    method crypto.generateCRMFRequest.  The array should have the
 *    3 arguments keySize, "keyParams", and "keyGenAlg" mentioned in
 *    the definition of crypto.generateCRMFRequest at the following
 *    document http://docs.iplanet.com/docs/manuals/psm/11/cmcjavascriptapi.html 
 *  keyGenType
 *    A structure used to store the information about the newly created
 *    key pair.
 *  uiCxt
 *    An interface requestor that would be used to get an nsIPrompt
 *    if we need to ask the user for a password.
 *  slotToUse
 *    The PKCS11 slot to use for generating the key pair. If nullptr, then
 *    this function should select a slot that can do the key generation 
 *    from the keytype associted with the keyPairInfo, and pass it back to
 *    the caller so that subsequence key generations can use the same slot. 
 *  willEscrow
 *    If true, then that means we will try to escrow the generated
 *    private key when building the CRMF request.  If false, then
 *    we will not try to escrow the private key.
 *
 * NOTES:
 * This function takes care of reading a set of 3 parameters that define
 * one key generation.  The argv pointer should be one that originates
 * from the argv parameter passed in to the method nsCrypto::GenerateCRMFRequest.
 * The function interprets the argument in the first index as an integer and
 * passes that as the key size for the key generation-this parameter is
 * mandatory.  The second parameter is read in as a string.  This value can
 * be null in JavaScript world and everything will still work.  The third
 * parameter is a mandatory string that indicates what kind of key to generate.
 * There should always be 1-to-1 correspondence between the strings compared
 * in the function cryptojs_interpret_key_gen_type and the strings listed in
 * document at http://docs.iplanet.com/docs/manuals/psm/11/cmcjavascriptapi.html 
 * under the definition of the method generateCRMFRequest, for the parameter
 * "keyGenAlgN".  After reading the parameters, the function then 
 * generates the key pairs passing the parameters parsed from the JavaScript i
 * routine.  
 *
 * RETURN:
 * NS_OK if creating the Key was successful.  Any other return value
 * indicates an error.
 */

static nsresult
cryptojs_ReadArgsAndGenerateKey(JSContext *cx,
                                JS::Value *argv,
                                nsKeyPairInfo *keyGenType,
                                nsIInterfaceRequestor *uiCxt,
                                PK11SlotInfo **slot, bool willEscrow)
{
  JSString  *jsString;
  JSAutoByteString params;
  int    keySize;
  nsresult  rv;

  if (!JSVAL_IS_INT(argv[0])) {
    JS_ReportError(cx, "%s%s", JS_ERROR,
                   "passed in non-integer for key size");
    return NS_ERROR_FAILURE;
  }
  keySize = JSVAL_TO_INT(argv[0]);
  if (!JSVAL_IS_NULL(argv[1])) {
    jsString = JS_ValueToString(cx,argv[1]);
    NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
    argv[1] = STRING_TO_JSVAL(jsString);
    params.encodeLatin1(cx, jsString);
    NS_ENSURE_TRUE(!!params, NS_ERROR_OUT_OF_MEMORY);
  }

  if (JSVAL_IS_NULL(argv[2])) {
    JS_ReportError(cx,"%s%s", JS_ERROR,
             "key generation type not specified");
    return NS_ERROR_FAILURE;
  }
  jsString = JS_ValueToString(cx, argv[2]);
  NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
  argv[2] = STRING_TO_JSVAL(jsString);
  nsDependentJSString dependentKeyGenAlg;
  NS_ENSURE_TRUE(dependentKeyGenAlg.init(cx, jsString), NS_ERROR_UNEXPECTED);
  nsAutoString keyGenAlg(dependentKeyGenAlg);
  keyGenAlg.Trim("\r\n\t ");
  keyGenType->keyGenType = cryptojs_interpret_key_gen_type(keyGenAlg);
  if (keyGenType->keyGenType == invalidKeyGen) {
    NS_LossyConvertUTF16toASCII keyGenAlgNarrow(dependentKeyGenAlg);
    JS_ReportError(cx, "%s%s%s", JS_ERROR,
                   "invalid key generation argument:",
                   keyGenAlgNarrow.get());
    goto loser;
  }
  if (!*slot) {
    *slot = nsGetSlotForKeyGen(keyGenType->keyGenType, uiCxt);
    if (!*slot)
      goto loser;
  }

  rv = cryptojs_generateOneKeyPair(cx,keyGenType,keySize,params.ptr(),uiCxt,
                                   *slot,willEscrow);

  if (rv != NS_OK) {
    NS_LossyConvertUTF16toASCII keyGenAlgNarrow(dependentKeyGenAlg);
    JS_ReportError(cx,"%s%s%s", JS_ERROR,
                   "could not generate the key for algorithm ",
                   keyGenAlgNarrow.get());
    goto loser;
  }
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;
}

//Utility funciton to free up the memory used by nsKeyPairInfo
//arrays.
static void
nsFreeKeyPairInfo(nsKeyPairInfo *keyids, int numIDs)
{
  NS_ASSERTION(keyids, "NULL pointer passed to nsFreeKeyPairInfo");
  if (!keyids)
    return;
  int i;
  for (i=0; i<numIDs; i++) {
    if (keyids[i].pubKey)
      SECKEY_DestroyPublicKey(keyids[i].pubKey);
    if (keyids[i].privKey)
      SECKEY_DestroyPrivateKey(keyids[i].privKey);
    if (keyids[i].ecPopCert)
      CERT_DestroyCertificate(keyids[i].ecPopCert);
    if (keyids[i].ecPopPubKey)
      SECKEY_DestroyPublicKey(keyids[i].ecPopPubKey);
  }
  delete []keyids;
}

//Utility funciton used to free the genertaed cert request messages
static void
nsFreeCertReqMessages(CRMFCertReqMsg **certReqMsgs, int32_t numMessages)
{
  int32_t i;
  for (i=0; i<numMessages && certReqMsgs[i]; i++) {
    CRMF_DestroyCertReqMsg(certReqMsgs[i]);
  }
  delete []certReqMsgs;
}

//If the form called for escrowing the private key we just generated,
//this function adds all the correct elements to the request.
//That consists of adding CRMFEncryptedKey to the reques as part
//of the CRMFPKIArchiveOptions Control.
static nsresult
nsSetEscrowAuthority(CRMFCertRequest *certReq, nsKeyPairInfo *keyInfo,
                     nsNSSCertificate *wrappingCert)
{
  if (!wrappingCert ||
      CRMF_CertRequestIsControlPresent(certReq, crmfPKIArchiveOptionsControl)){
    return NS_ERROR_FAILURE;
  }
  ScopedCERTCertificate cert(wrappingCert->GetCert());
  if (!cert)
    return NS_ERROR_FAILURE;

  CRMFEncryptedKey *encrKey = 
      CRMF_CreateEncryptedKeyWithEncryptedValue(keyInfo->privKey, cert);
  if (!encrKey)
    return NS_ERROR_FAILURE;

  CRMFPKIArchiveOptions *archOpt = 
      CRMF_CreatePKIArchiveOptions(crmfEncryptedPrivateKey, encrKey);
  if (!archOpt) {
    CRMF_DestroyEncryptedKey(encrKey);
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = CRMF_CertRequestSetPKIArchiveOptions(certReq, archOpt);
  CRMF_DestroyEncryptedKey(encrKey);
  CRMF_DestroyPKIArchiveOptions(archOpt);
  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

//Set the Distinguished Name (Subject Name) for the cert
//being requested.
static nsresult
nsSetDNForRequest(CRMFCertRequest *certReq, char *reqDN)
{
  if (!reqDN || CRMF_CertRequestIsFieldPresent(certReq, crmfSubject)) {
    return NS_ERROR_FAILURE;
  }
  ScopedCERTName subjectName(CERT_AsciiToName(reqDN));
  if (!subjectName) {
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = CRMF_CertRequestSetTemplateField(certReq, crmfSubject,
                                                   static_cast<void*>
                                                              (subjectName));
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

//Set Registration Token Control on the request.
static nsresult
nsSetRegToken(CRMFCertRequest *certReq, char *regToken)
{
  // this should never happen, but might as well add this.
  NS_ASSERTION(certReq, "A bogus certReq passed to nsSetRegToken");
  if (regToken){
    if (CRMF_CertRequestIsControlPresent(certReq, crmfRegTokenControl))
      return NS_ERROR_FAILURE;
  
    SECItem src;
    src.data = (unsigned char*)regToken;
    src.len  = strlen(regToken);
    SECItem *derEncoded = SEC_ASN1EncodeItem(nullptr, nullptr, &src, 
                                        SEC_ASN1_GET(SEC_UTF8StringTemplate));

    if (!derEncoded)
      return NS_ERROR_FAILURE;

    SECStatus srv = CRMF_CertRequestSetRegTokenControl(certReq, derEncoded);
    SECITEM_FreeItem(derEncoded,true);
    if (srv != SECSuccess)
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//Set the Authenticator control on the cert reuest.  It's just
//a string that gets passed along.
static nsresult
nsSetAuthenticator(CRMFCertRequest *certReq, char *authenticator)
{
  //This should never happen, but might as well check.
  NS_ASSERTION(certReq, "Bogus certReq passed to nsSetAuthenticator");
  if (authenticator) {
    if (CRMF_CertRequestIsControlPresent(certReq, crmfAuthenticatorControl))
      return NS_ERROR_FAILURE;
    
    SECItem src;
    src.data = (unsigned char*)authenticator;
    src.len  = strlen(authenticator);
    SECItem *derEncoded = SEC_ASN1EncodeItem(nullptr, nullptr, &src,
                                     SEC_ASN1_GET(SEC_UTF8StringTemplate));
    if (!derEncoded)
      return NS_ERROR_FAILURE;

    SECStatus srv = CRMF_CertRequestSetAuthenticatorControl(certReq, 
                                                            derEncoded);
    SECITEM_FreeItem(derEncoded, true);
    if (srv != SECSuccess)
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// ASN1 DER encoding rules say that when encoding a BIT string,
// the length in the header for the bit string is the number 
// of "useful" bits in the BIT STRING.  So the function finds
// it and sets accordingly for the returned item.
static void
nsPrepareBitStringForEncoding (SECItem *bitsmap, SECItem *value)
{
  unsigned char onebyte;
  unsigned int i, len = 0;

  /* to prevent warning on some platform at compile time */
  onebyte = '\0';
  /* Get the position of the right-most turn-on bit */
  for (i = 0; i < (value->len ) * 8; ++i) {
    if (i % 8 == 0)
      onebyte = value->data[i/8];
    if (onebyte & 0x80)
      len = i;
    onebyte <<= 1;
  }

  bitsmap->data = value->data;
  /* Add one here since we work with base 1 */
  bitsmap->len = len + 1;
}

//This next section defines all the functions that sets the 
//keyUsageExtension for all the different types of key gens
//we handle.  The keyUsageExtension is just a bit flag extension
//that we set in wrapper functions that call straight into
//nsSetKeyUsageExtension.  There is one wrapper funciton for each
//keyGenType.  The correct function will eventually be called 
//by going through a switch statement based on the nsKeyGenType
//in the nsKeyPairInfo struct.
static nsresult
nsSetKeyUsageExtension(CRMFCertRequest *crmfReq,
                       unsigned char   keyUsage)
{
  SECItem                 *encodedExt= nullptr;
  SECItem                  keyUsageValue = { (SECItemType) 0, nullptr, 0 };
  SECItem                  bitsmap = { (SECItemType) 0, nullptr, 0 };
  SECStatus                srv;
  CRMFCertExtension       *ext = nullptr;
  CRMFCertExtCreationInfo  extAddParams;
  SEC_ASN1Template         bitStrTemplate = {SEC_ASN1_BIT_STRING, 0, nullptr,
                                             sizeof(SECItem)};

  keyUsageValue.data = &keyUsage;
  keyUsageValue.len  = 1;
  nsPrepareBitStringForEncoding(&bitsmap, &keyUsageValue);

  encodedExt = SEC_ASN1EncodeItem(nullptr, nullptr, &bitsmap,&bitStrTemplate);
  if (!encodedExt) {
    goto loser;
  }
  ext = CRMF_CreateCertExtension(SEC_OID_X509_KEY_USAGE, true, encodedExt);
  if (!ext) {
      goto loser;
  }
  extAddParams.numExtensions = 1;
  extAddParams.extensions = &ext;
  srv = CRMF_CertRequestSetTemplateField(crmfReq, crmfExtension,
                                         &extAddParams);
  if (srv != SECSuccess) {
      goto loser;
  }
  CRMF_DestroyCertExtension(ext);
  SECITEM_FreeItem(encodedExt, true);
  return NS_OK;
 loser:
  if (ext) {
    CRMF_DestroyCertExtension(ext);
  }
  if (encodedExt) {
      SECITEM_FreeItem(encodedExt, true);
  }
  return NS_ERROR_FAILURE;
}

static nsresult
nsSetRSADualUse(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage =   KU_DIGITAL_SIGNATURE
                           | KU_NON_REPUDIATION
                           | KU_KEY_ENCIPHERMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSAKeyEx(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_KEY_ENCIPHERMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSASign(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE;


  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSANonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSASignNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE |
                           KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetECDualUse(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage =   KU_DIGITAL_SIGNATURE
                           | KU_NON_REPUDIATION
                           | KU_KEY_AGREEMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetECKeyEx(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_KEY_AGREEMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetECSign(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE;


  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetECNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetECSignNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE |
                           KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDH(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_KEY_AGREEMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSASign(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSANonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSASignNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE |
                           KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetKeyUsageExtension(CRMFCertRequest *crmfReq, nsKeyGenType keyGenType)
{
  nsresult rv;

  switch (keyGenType) {
  case rsaDualUse:
    rv = nsSetRSADualUse(crmfReq);
    break;
  case rsaEnc:
    rv = nsSetRSAKeyEx(crmfReq);
    break;
  case rsaSign:
    rv = nsSetRSASign(crmfReq);
    break;
  case rsaNonrepudiation:
    rv = nsSetRSANonRepudiation(crmfReq);
    break;
  case rsaSignNonrepudiation:
    rv = nsSetRSASignNonRepudiation(crmfReq);
    break;
  case ecDualUse:
    rv = nsSetECDualUse(crmfReq);
    break;
  case ecEnc:
    rv = nsSetECKeyEx(crmfReq);
    break;
  case ecSign:
    rv = nsSetECSign(crmfReq);
    break;
  case ecNonrepudiation:
    rv = nsSetECNonRepudiation(crmfReq);
    break;
  case ecSignNonrepudiation:
    rv = nsSetECSignNonRepudiation(crmfReq);
    break;
  case dhEx:
    rv = nsSetDH(crmfReq);
    break;
  case dsaSign:
    rv = nsSetDSASign(crmfReq);
    break;
  case dsaNonrepudiation:
    rv = nsSetDSANonRepudiation(crmfReq);
    break;
  case dsaSignNonrepudiation:
    rv = nsSetDSASignNonRepudiation(crmfReq);
    break;
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  return rv;
}

//Create a single CRMFCertRequest with all of the necessary parts 
//already installed.  The request returned by this function will
//have all the parts necessary and can just be added to a 
//Certificate Request Message.
static CRMFCertRequest*
nsCreateSingleCertReq(nsKeyPairInfo *keyInfo, char *reqDN, char *regToken, 
                      char *authenticator, nsNSSCertificate *wrappingCert)
{
  uint32_t reqID;
  nsresult rv;

  //The draft says the ID of the request should be a random
  //number.  We don't have a way of tracking this number
  //to compare when the reply actually comes back,though.
  PK11_GenerateRandom((unsigned char*)&reqID, sizeof(reqID));
  CRMFCertRequest *certReq = CRMF_CreateCertRequest(reqID);
  if (!certReq)
    return nullptr;

  long version = SEC_CERTIFICATE_VERSION_3;
  SECStatus srv;
  CERTSubjectPublicKeyInfo *spki = nullptr;
  srv = CRMF_CertRequestSetTemplateField(certReq, crmfVersion, &version);
  if (srv != SECSuccess)
    goto loser;
  
  spki = SECKEY_CreateSubjectPublicKeyInfo(keyInfo->pubKey);
  if (!spki)
    goto loser;

  srv = CRMF_CertRequestSetTemplateField(certReq, crmfPublicKey, spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (srv != SECSuccess)
    goto loser;

  if (wrappingCert && ns_can_escrow(keyInfo->keyGenType)) {
    rv = nsSetEscrowAuthority(certReq, keyInfo, wrappingCert);
    if (NS_FAILED(rv))
      goto loser;
  }
  rv = nsSetDNForRequest(certReq, reqDN);
  if (NS_FAILED(rv))
    goto loser;

  rv = nsSetRegToken(certReq, regToken);
  if (NS_FAILED(rv))
    goto loser;

  rv = nsSetAuthenticator(certReq, authenticator);
  if (NS_FAILED(rv))
    goto loser;

 rv = nsSetKeyUsageExtension(certReq, keyInfo->keyGenType); 
  if (NS_FAILED(rv))
    goto loser;

  return certReq;
loser:
  if (certReq) {
    CRMF_DestroyCertRequest(certReq);
  }
  return nullptr;
}

/*
 * This function will set the Proof Of Possession (POP) for a request
 * associated with a key pair intended to do Key Encipherment.  Currently
 * this means encryption only keys.
 */
static nsresult
nsSetKeyEnciphermentPOP(CRMFCertReqMsg *certReqMsg, bool isEscrowed)
{
  SECItem       bitString;
  unsigned char der[2];
  SECStatus     srv;

  if (isEscrowed) {
    /* For proof of possession on escrowed keys, we use the
     * this Message option of POPOPrivKey and include a zero
     * length bit string in the POP field.  This is OK because the encrypted
     * private key already exists as part of the PKIArchiveOptions
     * Control and that for all intents and purposes proves that
     * we do own the private key.
     */
    der[0] = 0x03; /*We've got a bit string          */
    der[1] = 0x00; /*We've got a 0 length bit string */
    bitString.data = der;
    bitString.len  = 2;
    srv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg, crmfThisMessage,
                                              crmfNoSubseqMess, &bitString);
  } else {
    /* If the encryption key is not being escrowed, then we set the 
     * Proof Of Possession to be a Challenge Response mechanism.
     */
    srv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg,
                                              crmfSubsequentMessage,
                                              crmfChallengeResp, nullptr);
  }
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

static void
nsCRMFEncoderItemCount(void *arg, const char *buf, unsigned long len);

static void
nsCRMFEncoderItemStore(void *arg, const char *buf, unsigned long len);

static nsresult
nsSet_EC_DHMAC_ProofOfPossession(CRMFCertReqMsg *certReqMsg, 
                                 nsKeyPairInfo  *keyInfo,
                                 CRMFCertRequest *certReq)
{
  // RFC 2511 Appendix A section 2 a) defines,
  // the "text" input for HMAC shall be the DER encoded version of
  // of the single cert request.
  // We'll produce that encoding and destroy it afterwards,
  // because when sending the complete package to the CA,
  // we'll use a different encoding, one that includes POP and
  // allows multiple requests to be sent in one step.

  unsigned long der_request_len = 0;
  ScopedSECItem der_request;

  if (SECSuccess != CRMF_EncodeCertRequest(certReq, 
                                           nsCRMFEncoderItemCount, 
                                           &der_request_len))
    return NS_ERROR_FAILURE;

  der_request = SECITEM_AllocItem(nullptr, nullptr, der_request_len);
  if (!der_request)
    return NS_ERROR_FAILURE;

  // set len in returned SECItem back to zero, because it will
  // be used as the destination offset inside the 
  // nsCRMFEncoderItemStore callback.

  der_request->len = 0;

  if (SECSuccess != CRMF_EncodeCertRequest(certReq, 
                                           nsCRMFEncoderItemStore, 
                                           der_request))
    return NS_ERROR_FAILURE;

  // RFC 2511 Appendix A section 2 c):
  // "A key K is derived from the shared secret Kec and the subject and
  //  issuer names in the CA's certificate as follows:
  //  K = SHA1(DER-encoded-subjectName | Kec | DER-encoded-issuerName)"

  ScopedPK11SymKey shared_secret;
  ScopedPK11SymKey subject_and_secret;
  ScopedPK11SymKey subject_and_secret_and_issuer;
  ScopedPK11SymKey sha1_of_subject_and_secret_and_issuer;

  shared_secret = 
    PK11_PubDeriveWithKDF(keyInfo->privKey, // SECKEYPrivateKey *privKey
                          keyInfo->ecPopPubKey,  // SECKEYPublicKey *pubKey
                          false, // bool isSender
                          nullptr, // SECItem *randomA
                          nullptr, // SECItem *randomB
                          CKM_ECDH1_DERIVE, // CK_MECHANISM_TYPE derive
                          CKM_CONCATENATE_DATA_AND_BASE, // CK_MECHANISM_TYPE target
                          CKA_DERIVE, // CK_ATTRIBUTE_TYPE operation
                          0, // int keySize
                          CKD_NULL, // CK_ULONG kdf
                          nullptr, // SECItem *sharedData
                          nullptr); // void *wincx

  if (!shared_secret)
    return NS_ERROR_FAILURE;

  CK_KEY_DERIVATION_STRING_DATA concat_data_base;
  concat_data_base.pData = keyInfo->ecPopCert->derSubject.data;
  concat_data_base.ulLen = keyInfo->ecPopCert->derSubject.len;
  SECItem concat_data_base_item;
  concat_data_base_item.data = (unsigned char*)&concat_data_base;
  concat_data_base_item.len = sizeof(CK_KEY_DERIVATION_STRING_DATA);

  subject_and_secret =
    PK11_Derive(shared_secret, // PK11SymKey *baseKey
                CKM_CONCATENATE_DATA_AND_BASE, // CK_MECHANISM_TYPE mechanism
                &concat_data_base_item, // SECItem *param
                CKM_CONCATENATE_BASE_AND_DATA, // CK_MECHANISM_TYPE target
                CKA_DERIVE, // CK_ATTRIBUTE_TYPE operation
                0); // int keySize

  if (!subject_and_secret)
    return NS_ERROR_FAILURE;

  CK_KEY_DERIVATION_STRING_DATA concat_base_data;
  concat_base_data.pData = keyInfo->ecPopCert->derSubject.data;
  concat_base_data.ulLen = keyInfo->ecPopCert->derSubject.len;
  SECItem concat_base_data_item;
  concat_base_data_item.data = (unsigned char*)&concat_base_data;
  concat_base_data_item.len = sizeof(CK_KEY_DERIVATION_STRING_DATA);

  subject_and_secret_and_issuer =
    PK11_Derive(subject_and_secret, // PK11SymKey *baseKey
                CKM_CONCATENATE_BASE_AND_DATA, // CK_MECHANISM_TYPE mechanism
                &concat_base_data_item, // SECItem *param
                CKM_SHA1_KEY_DERIVATION, // CK_MECHANISM_TYPE target
                CKA_DERIVE, // CK_ATTRIBUTE_TYPE operation
                0); // int keySize

  if (!subject_and_secret_and_issuer)
    return NS_ERROR_FAILURE;

  sha1_of_subject_and_secret_and_issuer =
    PK11_Derive(subject_and_secret_and_issuer, // PK11SymKey *baseKey
                CKM_SHA1_KEY_DERIVATION, // CK_MECHANISM_TYPE mechanism
                nullptr, // SECItem *param
                CKM_SHA_1_HMAC, // CK_MECHANISM_TYPE target
                CKA_SIGN, // CK_ATTRIBUTE_TYPE operation
                0); // int keySize

  if (!sha1_of_subject_and_secret_and_issuer)
    return NS_ERROR_FAILURE;

  PK11Context *context = nullptr;
  PK11ContextCleanerTrueParam context_cleaner(context);

  SECItem ignore;
  ignore.data = 0;
  ignore.len = 0;

  context = 
    PK11_CreateContextBySymKey(CKM_SHA_1_HMAC, // CK_MECHANISM_TYPE type
                               CKA_SIGN, // CK_ATTRIBUTE_TYPE operation
                               sha1_of_subject_and_secret_and_issuer, // PK11SymKey *symKey
                               &ignore); // SECItem *param

  if (!context)
    return NS_ERROR_FAILURE;

  if (SECSuccess != PK11_DigestBegin(context))
    return NS_ERROR_FAILURE;

  if (SECSuccess != 
      PK11_DigestOp(context, der_request->data, der_request->len))
    return NS_ERROR_FAILURE;

  ScopedAutoSECItem result_hmac_sha1_item(SHA1_LENGTH);

  if (SECSuccess !=
      PK11_DigestFinal(context, 
                       result_hmac_sha1_item.data, 
                       &result_hmac_sha1_item.len, 
                       SHA1_LENGTH))
    return NS_ERROR_FAILURE;

  if (SECSuccess !=
      CRMF_CertReqMsgSetKeyAgreementPOP(certReqMsg, crmfDHMAC,
                                        crmfNoSubseqMess, &result_hmac_sha1_item))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

static nsresult
nsSetProofOfPossession(CRMFCertReqMsg *certReqMsg, 
                       nsKeyPairInfo  *keyInfo,
                       CRMFCertRequest *certReq)
{
  // Depending on the type of cert request we'll try
  // POP mechanisms in different order,
  // and add the result to the cert request message.
  //
  // For any signing or dual use cert,
  //   try signing first,
  //   fall back to DHMAC if we can
  //     (EC cert requests that provide keygen param "popcert"),
  //   otherwise fail.
  //
  // For encryption only certs that get escrowed, this is sufficient.
  //
  // For encryption only certs, that are not being escrowed, 
  //   try DHMAC if we can 
  //     (EC cert requests that provide keygen param "popcert"),
  //   otherwise we'll indicate challenge response should be used.
  
  bool isEncryptionOnlyCertRequest = false;
  bool escrowEncryptionOnlyCert = false;
  
  switch (keyInfo->keyGenType)
  {
    case rsaEnc:
    case ecEnc:
      isEncryptionOnlyCertRequest = true;
      break;
    
    case rsaSign:
    case rsaDualUse:
    case rsaNonrepudiation:
    case rsaSignNonrepudiation:
    case ecSign:
    case ecDualUse:
    case ecNonrepudiation:
    case ecSignNonrepudiation:
    case dsaSign:
    case dsaNonrepudiation:
    case dsaSignNonrepudiation:
      break;
    
    case dhEx:
    /* This case may be supported in the future, but for now, we just fall 
      * though to the default case and return an error for diffie-hellman keys.
    */
    default:
      return NS_ERROR_FAILURE;
  };
    
  if (isEncryptionOnlyCertRequest)
  {
    escrowEncryptionOnlyCert = 
      CRMF_CertRequestIsControlPresent(certReq,crmfPKIArchiveOptionsControl);
  }
    
  bool gotDHMACParameters = false;
  
  if (isECKeyGenType(keyInfo->keyGenType) && 
      keyInfo->ecPopCert && 
      keyInfo->ecPopPubKey)
  {
    gotDHMACParameters = true;
  }
  
  if (isEncryptionOnlyCertRequest)
  {
    if (escrowEncryptionOnlyCert)
      return nsSetKeyEnciphermentPOP(certReqMsg, true); // escrowed
    
    if (gotDHMACParameters)
      return nsSet_EC_DHMAC_ProofOfPossession(certReqMsg, keyInfo, certReq);
    
    return nsSetKeyEnciphermentPOP(certReqMsg, false); // not escrowed
  }
  
  // !isEncryptionOnlyCertRequest
  
  SECStatus srv = CRMF_CertReqMsgSetSignaturePOP(certReqMsg,
                                                 keyInfo->privKey,
                                                 keyInfo->pubKey, nullptr,
                                                 nullptr, nullptr);

  if (srv == SECSuccess)
    return NS_OK;
  
  if (!gotDHMACParameters)
    return NS_ERROR_FAILURE;
  
  return nsSet_EC_DHMAC_ProofOfPossession(certReqMsg, keyInfo, certReq);
}

static void
nsCRMFEncoderItemCount(void *arg, const char *buf, unsigned long len)
{
  unsigned long *count = (unsigned long *)arg;
  *count += len;
}

static void
nsCRMFEncoderItemStore(void *arg, const char *buf, unsigned long len)
{
  SECItem *dest = (SECItem *)arg;
  memcpy(dest->data + dest->len, buf, len);
  dest->len += len;
}

static SECItem*
nsEncodeCertReqMessages(CRMFCertReqMsg **certReqMsgs)
{
  unsigned long len = 0;
  if (CRMF_EncodeCertReqMessages(certReqMsgs, nsCRMFEncoderItemCount, &len)
      != SECSuccess) {
    return nullptr;
  }
  SECItem *dest = (SECItem *)PORT_Alloc(sizeof(SECItem));
  if (!dest) {
    return nullptr;
  }
  dest->type = siBuffer;
  dest->data = (unsigned char *)PORT_Alloc(len);
  if (!dest->data) {
    PORT_Free(dest);
    return nullptr;
  }
  dest->len = 0;

  if (CRMF_EncodeCertReqMessages(certReqMsgs, nsCRMFEncoderItemStore, dest)
      != SECSuccess) {
    SECITEM_FreeItem(dest, true);
    return nullptr;
  }
  return dest;
}

//Create a Base64 encoded CRMFCertReqMsg that can be sent to a CA
//requesting one or more certificates to be issued.  This function
//creates a single cert request per key pair and then appends it to
//a message that is ultimately sent off to a CA.
static char*
nsCreateReqFromKeyPairs(nsKeyPairInfo *keyids, int32_t numRequests,
                        char *reqDN, char *regToken, char *authenticator,
                        nsNSSCertificate *wrappingCert) 
{
  // We'use the goto notation for clean-up purposes in this function
  // that calls the C API of NSS.
  int32_t i;
  // The ASN1 encoder in NSS wants the last entry in the array to be
  // nullptr so that it knows when the last element is.
  CRMFCertReqMsg **certReqMsgs = new CRMFCertReqMsg*[numRequests+1];
  CRMFCertRequest *certReq;
  if (!certReqMsgs)
    return nullptr;
  memset(certReqMsgs, 0, sizeof(CRMFCertReqMsg*)*(1+numRequests));
  SECStatus srv;
  nsresult rv;
  SECItem *encodedReq;
  char *retString;
  for (i=0; i<numRequests; i++) {
    certReq = nsCreateSingleCertReq(&keyids[i], reqDN, regToken, authenticator,
                                    wrappingCert);
    if (!certReq)
      goto loser;

    certReqMsgs[i] = CRMF_CreateCertReqMsg();
    if (!certReqMsgs[i])
      goto loser;
    srv = CRMF_CertReqMsgSetCertRequest(certReqMsgs[i], certReq);
    if (srv != SECSuccess)
      goto loser;

    rv = nsSetProofOfPossession(certReqMsgs[i], &keyids[i], certReq);
    if (NS_FAILED(rv))
      goto loser;
    CRMF_DestroyCertRequest(certReq);
  }
  encodedReq = nsEncodeCertReqMessages(certReqMsgs);
  nsFreeCertReqMessages(certReqMsgs, numRequests);

  retString = NSSBase64_EncodeItem (nullptr, nullptr, 0, encodedReq);
  SECITEM_FreeItem(encodedReq, true);
  return retString;
loser:
  nsFreeCertReqMessages(certReqMsgs,numRequests);
  return nullptr;;
}

static nsISupports *
GetISupportsFromContext(JSContext *cx)
{
    if (JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)
        return static_cast<nsISupports *>(JS_GetContextPrivate(cx));

    return nullptr;
}

//The top level method which is a member of nsIDOMCrypto
//for generate a base64 encoded CRMF request.
already_AddRefed<nsIDOMCRMFObject>
nsCrypto::GenerateCRMFRequest(JSContext* aContext,
                              const nsCString& aReqDN,
                              const nsCString& aRegToken,
                              const nsCString& aAuthenticator,
                              const nsCString& aEaCert,
                              const nsCString& aJsCallback,
                              const Sequence<JS::Value>& aArgs,
                              ErrorResult& aRv)
{
  nsNSSShutDownPreventionLock locker;
  nsCOMPtr<nsIDOMCRMFObject> crmf;
  nsresult nrv;

  uint32_t argc = aArgs.Length();

  /*
   * Get all of the parameters.
   */
  if (argc % 3 != 0) {
    aRv.ThrowNotEnoughArgsError();
    return nullptr;
  }

  if (aReqDN.IsVoid()) {
    NS_WARNING("no DN specified");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (aJsCallback.IsVoid()) {
    NS_WARNING("no completion function specified");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JS::RootedObject script_obj(aContext, GetWrapper());
  if (MOZ_UNLIKELY(!script_obj)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (!nsContentUtils::GetContentSecurityPolicy(aContext, getter_AddRefs(csp))) {
    NS_ERROR("Error: failed to get CSP");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool evalAllowed = true;
  bool reportEvalViolations = false;
  if (csp && NS_FAILED(csp->GetAllowsEval(&reportEvalViolations, &evalAllowed))) {
    NS_WARNING("CSP: failed to get allowsEval");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (reportEvalViolations) {
    NS_NAMED_LITERAL_STRING(scriptSample, "window.crypto.generateCRMFRequest: call to eval() or related function blocked by CSP");

    const char *fileName;
    uint32_t lineNum;
    nsJSUtils::GetCallingLocation(aContext, &fileName, &lineNum);
    csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                             NS_ConvertASCIItoUTF16(fileName),
                             scriptSample,
                             lineNum);
  }

  if (!evalAllowed) {
    NS_WARNING("eval() not allowed by Content Security Policy");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  //Put up some UI warning that someone is trying to
  //escrow the private key.
  //Don't addref this copy.  That way ths reference goes away
  //at the same the nsIX09Cert ref goes away.
  nsNSSCertificate *escrowCert = nullptr;
  nsCOMPtr<nsIX509Cert> nssCert;
  bool willEscrow = false;
  if (!aEaCert.IsVoid()) {
    SECItem certDer = {siBuffer, nullptr, 0};
    SECStatus srv = ATOB_ConvertAsciiToItem(&certDer, aEaCert.get());
    if (srv != SECSuccess) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    ScopedCERTCertificate cert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                       &certDer, nullptr,
                                                       false, true));
    if (!cert) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    escrowCert = nsNSSCertificate::Create(cert);
    nssCert = escrowCert;
    if (!nssCert) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    nsCOMPtr<nsIDOMCryptoDialogs> dialogs;
    nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                                NS_GET_IID(nsIDOMCryptoDialogs),
                                NS_DOMCRYPTODIALOGS_CONTRACTID);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }

    bool okay=false;
    {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        okay = false;
      }
      else {
        dialogs->ConfirmKeyEscrow(nssCert, &okay);
      }
    }
    if (!okay) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    willEscrow = true;
  }
  nsCOMPtr<nsIInterfaceRequestor> uiCxt = new PipUIContext;
  int32_t numRequests = argc / 3;
  nsKeyPairInfo *keyids = new nsKeyPairInfo[numRequests];
  memset(keyids, 0, sizeof(nsKeyPairInfo)*numRequests);
  int keyInfoIndex;
  uint32_t i;
  PK11SlotInfo *slot = nullptr;
  // Go through all of the arguments and generate the appropriate key pairs.
  for (i=0,keyInfoIndex=0; i<argc; i+=3,keyInfoIndex++) {
    nrv = cryptojs_ReadArgsAndGenerateKey(aContext,
                                          const_cast<JS::Value*>(&aArgs[i]),
                                          &keyids[keyInfoIndex],
                                          uiCxt, &slot, willEscrow);

    if (NS_FAILED(nrv)) {
      if (slot)
        PK11_FreeSlot(slot);
      nsFreeKeyPairInfo(keyids,numRequests);
      aRv.Throw(nrv);
      return nullptr;
    }
  }
  // By this time we'd better have a slot for the key gen.
  NS_ASSERTION(slot, "There was no slot selected for key generation");
  if (slot)
    PK11_FreeSlot(slot);

  char *encodedRequest = nsCreateReqFromKeyPairs(keyids,numRequests,
                                                 const_cast<char*>(aReqDN.get()),
                                                 const_cast<char*>(aRegToken.get()),
                                                 const_cast<char*>(aAuthenticator.get()),
                                                 escrowCert);
#ifdef DEBUG_javi
  printf ("Created the folloing CRMF request:\n%s\n", encodedRequest);
#endif
  if (!encodedRequest) {
    nsFreeKeyPairInfo(keyids, numRequests);
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsCRMFObject *newObject = new nsCRMFObject();
  newObject->SetCRMFRequest(encodedRequest);
  crmf = newObject;
  nsFreeKeyPairInfo(keyids, numRequests);

  // Post an event on the UI queue so that the JS gets called after
  // we return control to the JS layer.  Why do we have to this?
  // Because when this API was implemented for PSM 1.x w/ Communicator,
  // the only way to make this method work was to have a callback
  // in the JS layer that got called after key generation had happened.
  // So for backwards compatibility, we return control and then just post
  // an event to call the JS the script provides as the code to execute
  // when the request has been generated.
  //

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  if (MOZ_UNLIKELY(!secMan)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> principals;
  nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(principals));
  if (NS_FAILED(nrv)) {
    aRv.Throw(nrv);
    return nullptr;
  }
  if (MOZ_UNLIKELY(!principals)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCryptoRunArgs *args = new nsCryptoRunArgs();

  args->m_cx         = aContext;
  args->m_kungFuDeathGrip = GetISupportsFromContext(aContext);
  args->m_scope      = JS_GetParent(script_obj);
  if (!aJsCallback.IsVoid()) {
    args->m_jsCallback = aJsCallback;
  }
  args->m_principals = principals;

  nsCryptoRunnable *cryptoRunnable = new nsCryptoRunnable(args);

  rv = NS_DispatchToMainThread(cryptoRunnable);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    delete cryptoRunnable;
  }

  return crmf.forget();
}

// Reminder that we inherit the memory passed into us here.
// An implementation to let us back up certs as an event.
nsP12Runnable::nsP12Runnable(nsIX509Cert **certArr, int32_t numCerts,
                             nsIPK11Token *token)
{
  mCertArr  = certArr;
  mNumCerts = numCerts;
  mToken = token;
}

nsP12Runnable::~nsP12Runnable()
{
  int32_t i;
  for (i=0; i<mNumCerts; i++) {
      NS_IF_RELEASE(mCertArr[i]);
  }
  delete []mCertArr;
}


//Implementation that backs cert(s) into a PKCS12 file
NS_IMETHODIMP
nsP12Runnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "nsP12Runnable dispatched to the wrong thread");

  nsNSSShutDownPreventionLock locker;
  NS_ASSERTION(mCertArr, "certArr is NULL while trying to back up");

  nsString final;
  nsString temp;
  nsresult rv;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  //Build up the message that let's the user know we're trying to 
  //make PKCS12 backups of the new certs.
  nssComponent->GetPIPNSSBundleString("ForcedBackup1", final);
  final.Append(NS_LITERAL_STRING("\n\n").get());
  nssComponent->GetPIPNSSBundleString("ForcedBackup2", temp);
  final.Append(temp.get());
  final.Append(NS_LITERAL_STRING("\n\n").get());

  nssComponent->GetPIPNSSBundleString("ForcedBackup3", temp);

  final.Append(temp.get());
  nsNSSComponent::ShowAlertWithConstructedString(final);

  nsCOMPtr<nsIFilePicker> filePicker = 
                        do_CreateInstance("@mozilla.org/filepicker;1", &rv);
  if (!filePicker) {
    NS_ERROR("Could not create a file picker when backing up certs.");
    return rv;
  }

  nsCOMPtr<nsIWindowWatcher> wwatch =
    (do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> window;
  wwatch->GetActiveWindow(getter_AddRefs(window));

  nsString filePickMessage;
  nssComponent->GetPIPNSSBundleString("chooseP12BackupFileDialog",
                                      filePickMessage);
  rv = filePicker->Init(window, filePickMessage, nsIFilePicker::modeSave);
  NS_ENSURE_SUCCESS(rv, rv);

  filePicker->AppendFilter(NS_LITERAL_STRING("PKCS12"),
                           NS_LITERAL_STRING("*.p12"));
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  int16_t dialogReturn;
  filePicker->Show(&dialogReturn);
  if (dialogReturn == nsIFilePicker::returnCancel)
    return NS_OK;  //User canceled.  It'd be nice if they couldn't, 
                   //but oh well.

  nsCOMPtr<nsIFile> localFile;
  rv = filePicker->GetFile(getter_AddRefs(localFile));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsPKCS12Blob p12Cxt;
  
  p12Cxt.SetToken(mToken);
  p12Cxt.ExportToFile(localFile, mCertArr, mNumCerts);
  return NS_OK;
}

nsCryptoRunArgs::nsCryptoRunArgs() 
{
}
nsCryptoRunArgs::~nsCryptoRunArgs() {}


nsCryptoRunnable::nsCryptoRunnable(nsCryptoRunArgs *args)
{
  nsNSSShutDownPreventionLock locker;
  NS_ASSERTION(args,"Passed nullptr to nsCryptoRunnable constructor.");
  m_args = args;
  NS_IF_ADDREF(m_args);
  JS_AddNamedObjectRoot(args->m_cx, &args->m_scope,"nsCryptoRunnable::mScope");
}

nsCryptoRunnable::~nsCryptoRunnable()
{
  nsNSSShutDownPreventionLock locker;

  {
    JSAutoRequest ar(m_args->m_cx);
    JS_RemoveObjectRoot(m_args->m_cx, &m_args->m_scope);
  }

  NS_IF_RELEASE(m_args);
}

//Implementation that runs the callback passed to 
//crypto.generateCRMFRequest as an event.
NS_IMETHODIMP
nsCryptoRunnable::Run()
{
  nsNSSShutDownPreventionLock locker;
  AutoPushJSContext cx(m_args->m_cx);
  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, m_args->m_scope);

  bool ok =
    JS_EvaluateScriptForPrincipals(cx, m_args->m_scope,
                                   nsJSPrincipals::get(m_args->m_principals),
                                   m_args->m_jsCallback, 
                                   strlen(m_args->m_jsCallback),
                                   nullptr, 0, nullptr);
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

//Quick helper function to check if a newly issued cert
//already exists in the user's database.
static bool
nsCertAlreadyExists(SECItem *derCert)
{
  CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
  bool retVal = false;

  ScopedCERTCertificate cert(CERT_FindCertByDERCert(handle, derCert));
  if (cert) {
    if (cert->isperm && !cert->nickname && !cert->emailAddr) {
      //If the cert doesn't have a nickname or email addr, it is
      //bogus cruft, so delete it.
      SEC_DeletePermCertificate(cert);
    } else if (cert->isperm) {
      retVal = true;
    }
  }
  return retVal;
}

static int32_t
nsCertListCount(CERTCertList *certList)
{
  int32_t numCerts = 0;
  CERTCertListNode *node;

  node = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(node, certList)) {
    numCerts++;
    node = CERT_LIST_NEXT(node);
  }
  return numCerts;
}

//Import user certificates that arrive as a CMMF base64 encoded
//string.
void
nsCrypto::ImportUserCertificates(const nsAString& aNickname,
                                 const nsAString& aCmmfResponse,
                                 bool aDoForcedBackup,
                                 nsAString& aReturn,
                                 ErrorResult& aRv)
{
  nsNSSShutDownPreventionLock locker;
  char *nickname=nullptr, *cmmfResponse=nullptr;
  CMMFCertRepContent *certRepContent = nullptr;
  int numResponses = 0;
  nsIX509Cert **certArr = nullptr;
  int i;
  CMMFCertResponse *currResponse;
  CMMFPKIStatus reqStatus;
  CERTCertificate *currCert;
  PK11SlotInfo *slot;
  nsAutoCString localNick;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPK11Token> token;

  nickname = ToNewCString(aNickname);
  cmmfResponse = ToNewCString(aCmmfResponse);
  if (nsCRT::strcmp("null", nickname) == 0) {
    nsMemory::Free(nickname);
    nickname = nullptr;
  }

  SECItem cmmfDer = {siBuffer, nullptr, 0};
  SECStatus srv = ATOB_ConvertAsciiToItem(&cmmfDer, cmmfResponse);

  if (srv != SECSuccess) {
    rv = NS_ERROR_FAILURE;
    goto loser;
  }

  certRepContent = CMMF_CreateCertRepContentFromDER(CERT_GetDefaultCertDB(),
                                                    (const char*)cmmfDer.data,
                                                    cmmfDer.len);
  if (!certRepContent) {
    rv = NS_ERROR_FAILURE;
    goto loser;
  }

  numResponses = CMMF_CertRepContentGetNumResponses(certRepContent);

  if (aDoForcedBackup) {
    //We've been asked to force the user to back up these
    //certificates.  Let's keep an array of them around which
    //we pass along to the nsP12Runnable to use.
    certArr = new nsIX509Cert*[numResponses];
    // If this is NULL, chances are we're gonna fail really soon,
    // but let's try to keep going just in case.
    if (!certArr)
      aDoForcedBackup = false;

    memset(certArr, 0, sizeof(nsIX509Cert*)*numResponses);
  }
  for (i=0; i<numResponses; i++) {
    currResponse = CMMF_CertRepContentGetResponseAtIndex(certRepContent,i);
    if (!currResponse) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    reqStatus = CMMF_CertResponseGetPKIStatusInfoStatus(currResponse);
    if (!(reqStatus == cmmfGranted || reqStatus == cmmfGrantedWithMods)) {
      // The CA didn't give us the cert we requested.
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    currCert = CMMF_CertResponseGetCertificate(currResponse, 
                                               CERT_GetDefaultCertDB());
    if (!currCert) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }

    if (nsCertAlreadyExists(&currCert->derCert)) {
      if (aDoForcedBackup) {
        certArr[i] = nsNSSCertificate::Create(currCert);
        if (!certArr[i])
          goto loser;
        NS_ADDREF(certArr[i]);
      }
      CERT_DestroyCertificate(currCert);
      CMMF_DestroyCertResponse(currResponse);
      continue;
    }
    // Let's figure out which nickname to give the cert.  If 
    // a certificate with the same subject name already exists,
    // then just use that one, otherwise, get the default nickname.
    if (currCert->nickname) {
      localNick = currCert->nickname;
    }
    else if (!nickname || nickname[0] == '\0') {
      nsNSSCertificateDB::get_default_nickname(currCert, ctx, localNick);
    } else {
      //This is the case where we're getting a brand new
      //cert that doesn't have the same subjectName as a cert
      //that already exists in our db and the CA page has 
      //designated a nickname to use for the newly issued cert.
      localNick = nickname;
    }
    {
      char *cast_const_away = const_cast<char*>(localNick.get());
      slot = PK11_ImportCertForKey(currCert, cast_const_away, ctx);
    }
    if (!slot) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    if (aDoForcedBackup) {
      certArr[i] = nsNSSCertificate::Create(currCert);
      if (!certArr[i])
        goto loser;
      NS_ADDREF(certArr[i]);
    }
    CERT_DestroyCertificate(currCert);

    if (!token)
      token = new nsPK11Token(slot);

    PK11_FreeSlot(slot);
    CMMF_DestroyCertResponse(currResponse);
  }
  //Let the loser: label take care of freeing up our reference to
  //nickname (This way we don't free it twice and avoid crashing.
  //That would be a good thing.

  //Import the root chain into the cert db.
 {
  ScopedCERTCertList caPubs(CMMF_CertRepContentGetCAPubs(certRepContent));
  if (caPubs) {
    int32_t numCAs = nsCertListCount(caPubs);
    
    NS_ASSERTION(numCAs > 0, "Invalid number of CA's");
    if (numCAs > 0) {
      CERTCertListNode *node;
      SECItem *derCerts;

      derCerts = static_cast<SECItem*>
                            (nsMemory::Alloc(sizeof(SECItem)*numCAs));
      if (!derCerts) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto loser;
      }
      for (node = CERT_LIST_HEAD(caPubs), i=0; 
           !CERT_LIST_END(node, caPubs);
           node = CERT_LIST_NEXT(node), i++) {
        derCerts[i] = node->cert->derCert;
      }
      nsNSSCertificateDB::ImportValidCACerts(numCAs, derCerts, ctx);
      nsMemory::Free(derCerts);
    }
  }
 }

  if (aDoForcedBackup) {
    // I can't pop up a file picker from the depths of JavaScript,
    // so I'll just post an event on the UI queue to do the backups
    // later.
    nsCOMPtr<nsIRunnable> p12Runnable = new nsP12Runnable(certArr, numResponses,
                                                          token);
    if (!p12Runnable) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }

    // null out the certArr pointer which has now been inherited by
    // the nsP12Runnable instance so that we don't free up the
    // memory on the way out.
    certArr = nullptr;

    rv = NS_DispatchToMainThread(p12Runnable);
    if (NS_FAILED(rv))
      goto loser;
  }

 loser:
  if (certArr) {
    for (i=0; i<numResponses; i++) {
      NS_IF_RELEASE(certArr[i]);
    }
    delete []certArr;
  }
  aReturn.Assign(EmptyString());
  if (nickname) {
    NS_Free(nickname);
  }
  if (cmmfResponse) {
    NS_Free(cmmfResponse);
  }
  if (certRepContent) {
    CMMF_DestroyCertRepContent(certRepContent);
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
nsCrypto::PopChallengeResponse(const nsAString& aChallenge,
                               nsAString& aReturn,
                               ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
nsCrypto::Random(int32_t aNumBytes, nsAString& aReturn, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

static void
GetDocumentFromContext(JSContext *cx, nsIDocument **aDocument)
{
  // Get the script context.
  nsIScriptContext* scriptContext = GetScriptContextFromJSContext(cx);
  if (!scriptContext) {
    return;
  }

  nsCOMPtr<nsIDOMWindow> domWindow = 
    do_QueryInterface(scriptContext->GetGlobalObject());
  if (!domWindow) {
    return;
  }

  nsCOMPtr<nsIDOMDocument> domDocument;
  domWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument) {
    return;
  }

  CallQueryInterface(domDocument, aDocument);

  return;
}

void signTextOutputCallback(void *arg, const char *buf, unsigned long len)
{
  ((nsCString*)arg)->Append(buf, len);
}

void
nsCrypto::SignText(JSContext* aContext,
                   const nsAString& aStringToSign,
                   const nsAString& aCaOption,
                   const Sequence<nsCString>& aArgs,
                   nsAString& aReturn)
{
  // XXX This code should return error codes, but we're keeping this
  //     backwards compatible with NS4.x and so we can't throw exceptions.
  NS_NAMED_LITERAL_STRING(internalError, "error:internalError");

  aReturn.Truncate();

  uint32_t argc = aArgs.Length();

  if (!aCaOption.EqualsLiteral("auto") &&
      !aCaOption.EqualsLiteral("ask")) {
    NS_WARNING("caOption argument must be ask or auto");
    aReturn.Append(internalError);

    return;
  }

  // It was decided to always behave as if "ask" were specified.
  // XXX Should we warn in the JS Console for auto?

  nsCOMPtr<nsIInterfaceRequestor> uiContext = new PipUIContext;
  if (!uiContext) {
    aReturn.Append(internalError);

    return;
  }

  bool bestOnly = true;
  bool validOnly = true;
  CERTCertList* certList =
    CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), certUsageEmailSigner,
                              bestOnly, validOnly, uiContext);

  uint32_t numCAs = argc;
  if (numCAs > 0) {
    nsAutoArrayPtr<char*> caNames(new char*[numCAs]);
    if (!caNames) {
      aReturn.Append(internalError);
      return;
    }

    uint32_t i;
    for (i = 0; i < numCAs; ++i)
      caNames[i] = const_cast<char*>(aArgs[i].get());

    if (certList &&
        CERT_FilterCertListByCANames(certList, numCAs, caNames,
                                     certUsageEmailSigner) != SECSuccess) {
      aReturn.Append(internalError);

      return;
    }
  }

  if (!certList || CERT_LIST_EMPTY(certList)) {
    aReturn.AppendLiteral("error:noMatchingCert");

    return;
  }

  nsCOMPtr<nsIFormSigningDialog> fsd =
    do_CreateInstance(NS_FORMSIGNINGDIALOG_CONTRACTID);
  if (!fsd) {
    aReturn.Append(internalError);

    return;
  }

  nsCOMPtr<nsIDocument> document;
  GetDocumentFromContext(aContext, getter_AddRefs(document));
  if (!document) {
    aReturn.Append(internalError);

    return;
  }

  // Get the hostname from the URL of the document.
  nsIURI* uri = document->GetDocumentURI();
  if (!uri) {
    aReturn.Append(internalError);

    return;
  }

  nsresult rv;

  nsCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    aReturn.Append(internalError);

    return;
  }

  int32_t numberOfCerts = 0;
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    ++numberOfCerts;
  }

  ScopedCERTCertNicknames nicknames(getNSSCertNicknamesFromCertList(certList));

  if (!nicknames) {
    aReturn.Append(internalError);

    return;
  }

  NS_ASSERTION(nicknames->numnicknames == numberOfCerts,
               "nicknames->numnicknames != numberOfCerts");

  nsAutoArrayPtr<PRUnichar*> certNicknameList(new PRUnichar*[nicknames->numnicknames * 2]);
  if (!certNicknameList) {
    aReturn.Append(internalError);

    return;
  }

  PRUnichar** certDetailsList = certNicknameList.get() + nicknames->numnicknames;

  int32_t certsToUse;
  for (node = CERT_LIST_HEAD(certList), certsToUse = 0;
       !CERT_LIST_END(node, certList) && certsToUse < nicknames->numnicknames;
       node = CERT_LIST_NEXT(node)) {
    RefPtr<nsNSSCertificate> tempCert(nsNSSCertificate::Create(node->cert));
    if (tempCert) {
      nsAutoString nickWithSerial, details;
      rv = tempCert->FormatUIStrings(NS_ConvertUTF8toUTF16(nicknames->nicknames[certsToUse]),
                                     nickWithSerial, details);
      if (NS_SUCCEEDED(rv)) {
        certNicknameList[certsToUse] = ToNewUnicode(nickWithSerial);
        if (certNicknameList[certsToUse]) {
          certDetailsList[certsToUse] = ToNewUnicode(details);
          if (!certDetailsList[certsToUse]) {
            nsMemory::Free(certNicknameList[certsToUse]);
            continue;
          }
          ++certsToUse;
        }
      }
    }
  }

  if (certsToUse == 0) {
    aReturn.Append(internalError);

    return;
  }

  NS_ConvertUTF8toUTF16 utf16Host(host);

  CERTCertificate *signingCert = nullptr;
  bool tryAgain, canceled;
  nsAutoString password;
  do {
    // Throw up the form signing confirmation dialog and get back the index
    // of the selected cert.
    int32_t selectedIndex = -1;
    rv = fsd->ConfirmSignText(uiContext, utf16Host, aStringToSign,
                              const_cast<const PRUnichar**>(certNicknameList.get()),
                              const_cast<const PRUnichar**>(certDetailsList),
                              certsToUse, &selectedIndex, password,
                              &canceled);
    if (NS_FAILED(rv) || canceled) {
      break; // out of tryAgain loop
    }

    int32_t j = 0;
    for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
         node = CERT_LIST_NEXT(node)) {
      if (j == selectedIndex) {
        signingCert = CERT_DupCertificate(node->cert);
        break; // out of cert list iteration loop
      }
      ++j;
    }

    if (!signingCert) {
      rv = NS_ERROR_FAILURE;
      break; // out of tryAgain loop
    }

    NS_ConvertUTF16toUTF8 pwUtf8(password);

    tryAgain =
      PK11_CheckUserPassword(signingCert->slot,
                             const_cast<char *>(pwUtf8.get())) != SECSuccess;
    // XXX we should show an error dialog before retrying
  } while (tryAgain);

  int32_t k;
  for (k = 0; k < certsToUse; ++k) {
    nsMemory::Free(certNicknameList[k]);
    nsMemory::Free(certDetailsList[k]);
  }

  if (NS_FAILED(rv)) { // something went wrong inside the tryAgain loop
    aReturn.Append(internalError);

    return;
  }

  if (canceled) {
    aReturn.AppendLiteral("error:userCancel");

    return;
  }

  SECKEYPrivateKey* privKey = PK11_FindKeyByAnyCert(signingCert, uiContext);
  if (!privKey) {
    aReturn.Append(internalError);

    return;
  }

  nsAutoCString charset(document->GetDocumentCharacterSet());

  // XXX Doing what nsFormSubmission::GetEncoder does (see
  //     http://bugzilla.mozilla.org/show_bug.cgi?id=81203).
  if (charset.EqualsLiteral("ISO-8859-1")) {
    charset.AssignLiteral("windows-1252");
  }

  nsCOMPtr<nsISaveAsCharset> encoder =
    do_CreateInstance(NS_SAVEASCHARSET_CONTRACTID);
  if (encoder) {
    rv = encoder->Init(charset.get(),
                       (nsISaveAsCharset::attr_EntityAfterCharsetConv + 
                       nsISaveAsCharset::attr_FallbackDecimalNCR),
                       0);
  }

  nsXPIDLCString buffer;
  if (aStringToSign.Length() > 0) {
    if (encoder && NS_SUCCEEDED(rv)) {
      rv = encoder->Convert(PromiseFlatString(aStringToSign).get(),
                            getter_Copies(buffer));
      if (NS_FAILED(rv)) {
        aReturn.Append(internalError);

        return;
      }
    }
    else {
      AppendUTF16toUTF8(aStringToSign, buffer);
    }
  }

  HASHContext *hc = HASH_Create(HASH_AlgSHA1);
  if (!hc) {
    aReturn.Append(internalError);

    return;
  }

  unsigned char hash[SHA1_LENGTH];

  SECItem digest;
  digest.data = hash;

  HASH_Begin(hc);
  HASH_Update(hc, reinterpret_cast<const unsigned char*>(buffer.get()),
              buffer.Length());
  HASH_End(hc, digest.data, &digest.len, SHA1_LENGTH);
  HASH_Destroy(hc);

  nsCString p7;
  SECStatus srv = SECFailure;

  SEC_PKCS7ContentInfo *ci = SEC_PKCS7CreateSignedData(signingCert,
                                                       certUsageEmailSigner,
                                                       nullptr, SEC_OID_SHA1,
                                                       &digest, nullptr, uiContext);
  if (ci) {
    srv = SEC_PKCS7IncludeCertChain(ci, nullptr);
    if (srv == SECSuccess) {
      srv = SEC_PKCS7AddSigningTime(ci);
      if (srv == SECSuccess) {
        srv = SEC_PKCS7Encode(ci, signTextOutputCallback, &p7, nullptr, nullptr,
                              uiContext);
      }
    }

    SEC_PKCS7DestroyContentInfo(ci);
  }

  if (srv != SECSuccess) {
    aReturn.Append(internalError);

    return;
  }

  SECItem binary_item;
  binary_item.data = reinterpret_cast<unsigned char*>
                                     (const_cast<char*>(p7.get()));
  binary_item.len = p7.Length();

  char *result = NSSBase64_EncodeItem(nullptr, nullptr, 0, &binary_item);
  if (result) {
    AppendASCIItoUTF16(result, aReturn);
  }
  else {
    aReturn.Append(internalError);
  }

  PORT_Free(result);

  return;
}

void
nsCrypto::Logout(ErrorResult& aRv)
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  {
    nsNSSShutDownPreventionLock locker;
    PK11_LogoutAll();
    SSL_ClearSessionCache();
  }

  rv = nssComponent->LogoutAuthenticatedPK11();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
nsCrypto::DisableRightClick(ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

nsCRMFObject::nsCRMFObject()
{
}

nsCRMFObject::~nsCRMFObject()
{
}

nsresult
nsCRMFObject::init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsCRMFObject::GetRequest(nsAString& aRequest)
{
  aRequest.Assign(mBase64Request);
  return NS_OK;
}

nsresult
nsCRMFObject::SetCRMFRequest(char *inRequest)
{
  mBase64Request.AssignWithConversion(inRequest);  
  return NS_OK;
}

#endif // MOZ_DISABLE_CRYPTOLEGACY

nsPkcs11::nsPkcs11()
{
}

nsPkcs11::~nsPkcs11()
{
}

//Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
nsPkcs11::DeleteModule(const nsAString& aModuleName)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  nsString errorMessage;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  if (aModuleName.IsEmpty()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  NS_ConvertUTF16toUTF8 modName(aModuleName);
  int32_t modType;
  SECStatus srv = SECMOD_DeleteModule(modName.get(), &modType);
  if (srv == SECSuccess) {
    SECMODModule *module = SECMOD_FindModule(modName.get());
    if (module) {
#ifndef MOZ_DISABLE_CRYPTOLEGACY
      nssComponent->ShutdownSmartCardThread(module);
#endif
      SECMOD_DestroyModule(module);
    }
    rv = NS_OK;
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

//Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
nsPkcs11::AddModule(const nsAString& aModuleName, 
                    const nsAString& aLibraryFullPath, 
                    int32_t aCryptoMechanismFlags, 
                    int32_t aCipherFlags)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  NS_ConvertUTF16toUTF8 moduleName(aModuleName);
  nsCString fullPath;
  // NSS doesn't support Unicode path.  Use native charset
  NS_CopyUnicodeToNative(aLibraryFullPath, fullPath);
  uint32_t mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  uint32_t cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleName.get(), fullPath.get(), 
                                      mechFlags, cipherFlags);
  if (srv == SECSuccess) {
    SECMODModule *module = SECMOD_FindModule(moduleName.get());
    if (module) {
#ifndef MOZ_DISABLE_CRYPTOLEGACY
      nssComponent->LaunchSmartCardThread(module);
#endif
      SECMOD_DestroyModule(module);
    }
  }

  // The error message we report to the user depends directly on 
  // what the return value for SEDMOD_AddNewModule is
  switch (srv) {
  case SECSuccess:
    return NS_OK;
  case SECFailure:
    return NS_ERROR_FAILURE;
  case -2:
    return NS_ERROR_ILLEGAL_VALUE;
  }
  NS_ERROR("Bogus return value, this should never happen");
  return NS_ERROR_FAILURE;
}

