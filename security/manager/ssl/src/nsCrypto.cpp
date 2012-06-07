/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsNSSComponent.h"
#include "nsCrypto.h"
#include "nsKeygenHandler.h"
#include "nsKeygenThread.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsThreadUtils.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsAutoPtr.h"
#include "nsAlgorithm.h"
#include "nsCRT.h"
#include "prprf.h"
#include "prmem.h"
#include "nsDOMCID.h"
#include "nsIDOMWindow.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMJSUtils.h"
#include "nsIXPConnect.h"
#include "nsIRunnable.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIFilePicker.h"
#include "nsJSPrincipals.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsXPIDLString.h"
#include "nsIGenKeypairInfoDlg.h"
#include "nsIDOMCryptoDialogs.h"
#include "nsIFormSigningDialog.h"
#include "nsIJSContextStack.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include <ctype.h>
#include "nsReadableUtils.h"
#include "pk11func.h"
#include "keyhi.h"
#include "cryptohi.h"
#include "seccomon.h"
#include "secerr.h"
#include "sechash.h"
extern "C" {
#include "crmf.h"
#include "pk11pqg.h"
}
#include "cmmf.h"
#include "nssb64.h"
#include "base64.h"
#include "cert.h"
#include "certdb.h"
#include "secmod.h"
#include "nsISaveAsCharset.h"

#include "ssl.h" // For SSL_ClearSessionCache

#include "nsNSSCleaner.h"
NSSCleanupAutoPtrClass(SECKEYPrivateKey, SECKEY_DestroyPrivateKey)
NSSCleanupAutoPtrClass(PK11SlotInfo, PK11_FreeSlot)
NSSCleanupAutoPtrClass(CERTCertNicknames, CERT_FreeNicknames)
NSSCleanupAutoPtrClass(PK11SymKey, PK11_FreeSymKey)
NSSCleanupAutoPtrClass_WithParam(PK11Context, PK11_DestroyContext, TrueParam, true)
NSSCleanupAutoPtrClass_WithParam(SECItem, SECITEM_FreeItem, TrueParam, true)

#include "nsNSSShutDown.h"
#include "nsNSSCertHelper.h"

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
  nsP12Runnable(nsIX509Cert **certArr, PRInt32 numCerts, nsIPK11Token *token);
  virtual ~nsP12Runnable();

  NS_IMETHOD Run();
  NS_DECL_ISUPPORTS
private:
  nsCOMPtr<nsIPK11Token> mToken;
  nsIX509Cert **mCertArr;
  PRInt32       mNumCerts;
};

// QueryInterface implementation for nsCrypto
NS_INTERFACE_MAP_BEGIN(nsCrypto)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Crypto)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCrypto)
NS_IMPL_RELEASE(nsCrypto)

// QueryInterface implementation for nsCRMFObject
NS_INTERFACE_MAP_BEGIN(nsCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CRMFObject)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCRMFObject)
NS_IMPL_RELEASE(nsCRMFObject)

// QueryInterface implementation for nsPkcs11
NS_INTERFACE_MAP_BEGIN(nsPkcs11)
  NS_INTERFACE_MAP_ENTRY(nsIPKCS11)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPkcs11)
NS_IMPL_RELEASE(nsPkcs11)

// ISupports implementation for nsCryptoRunnable
NS_IMPL_ISUPPORTS1(nsCryptoRunnable, nsIRunnable)

// ISupports implementation for nsP12Runnable
NS_IMPL_ISUPPORTS1(nsP12Runnable, nsIRunnable)

// ISupports implementation for nsCryptoRunArgs
NS_IMPL_ISUPPORTS0(nsCryptoRunArgs)

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

nsCrypto::nsCrypto() :
  mEnableSmartCardEvents(false)
{
}

nsCrypto::~nsCrypto()
{
}

NS_IMETHODIMP
nsCrypto::SetEnableSmartCardEvents(bool aEnable)
{
  nsresult rv = NS_OK;

  // this has the side effect of starting the nssComponent (and initializing
  // NSS) even if it isn't already going. Starting the nssComponent is a 
  // prerequisite for getting smartCard events.
  if (aEnable) {
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  mEnableSmartCardEvents = aEnable;
  return NS_OK;
}

NS_IMETHODIMP
nsCrypto::GetEnableSmartCardEvents(bool *aEnable)
{
  *aEnable = mEnableSmartCardEvents;
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
NS_IMETHODIMP
nsCrypto::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(NS_LITERAL_STRING(PSM_VERSION_STRING).get());
  return NS_OK;
}

/*
 * Given an nsKeyGenType, return the PKCS11 mechanism that will
 * perform the correct key generation.
 */
static PRUint32
cryptojs_convert_to_mechanism(nsKeyGenType keyGenType)
{
  PRUint32 retMech;

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
 * This function converts a string read through JavaScript parameters
 * and translates it to the internal enumeration representing the
 * key gen type.
 */
static nsKeyGenType
cryptojs_interpret_key_gen_type(char *keyAlg)
{
  char *end;
  if (keyAlg == nsnull) {
    return invalidKeyGen;
  }
  /* First let's remove all leading and trailing white space */
  while (isspace(keyAlg[0])) keyAlg++;
  end = strchr(keyAlg, '\0');
  if (end == nsnull) {
    return invalidKeyGen;
  }
  end--;
  while (isspace(*end)) end--;
  end[1] = '\0';
  if (strcmp(keyAlg, "rsa-ex") == 0) {
    return rsaEnc;
  } else if (strcmp(keyAlg, "rsa-dual-use") == 0) {
    return rsaDualUse;
  } else if (strcmp(keyAlg, "rsa-sign") == 0) {
    return rsaSign;
  } else if (strcmp(keyAlg, "rsa-sign-nonrepudiation") == 0) {
    return rsaSignNonrepudiation;
  } else if (strcmp(keyAlg, "rsa-nonrepudiation") == 0) {
    return rsaNonrepudiation;
  } else if (strcmp(keyAlg, "ec-ex") == 0) {
    return ecEnc;
  } else if (strcmp(keyAlg, "ec-dual-use") == 0) {
    return ecDualUse;
  } else if (strcmp(keyAlg, "ec-sign") == 0) {
    return ecSign;
  } else if (strcmp(keyAlg, "ec-sign-nonrepudiation") == 0) {
    return ecSignNonrepudiation;
  } else if (strcmp(keyAlg, "ec-nonrepudiation") == 0) {
    return ecNonrepudiation;
  } else if (strcmp(keyAlg, "dsa-sign-nonrepudiation") == 0) {
    return dsaSignNonrepudiation;
  } else if (strcmp(keyAlg, "dsa-sign") ==0 ){
    return dsaSign;
  } else if (strcmp(keyAlg, "dsa-nonrepudiation") == 0) {
    return dsaNonrepudiation;
  } else if (strcmp(keyAlg, "dh-ex") == 0) {
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
nsConvertToActualKeyGenParams(PRUint32 keyGenMech, char *params,
                              PRUint32 paramLen, PRInt32 keySize,
                              nsKeyPairInfo *keyPairInfo)
{
  void *returnParams = nsnull;


  switch (keyGenMech) {
  case CKM_RSA_PKCS_KEY_PAIR_GEN:
  {
    // For RSA, we don't support passing in key generation arguments from
    // the JS code just yet.
    if (params)
      return nsnull;

    PK11RSAGenParams *rsaParams;
    rsaParams = static_cast<PK11RSAGenParams*>
                           (nsMemory::Alloc(sizeof(PK11RSAGenParams)));
                              
    if (rsaParams == nsnull) {
      return nsnull;
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

    char *curve = nsnull;

    {
      // extract components of name=value list

      char *next_input = params;
      char *name = nsnull;
      char *value = nsnull;
      int name_len = 0;
      int value_len = 0;
  
      while (getNextNameValueFromECKeygenParamString(
              next_input, name, name_len, value, value_len,
              next_input))
      {
        if (PL_strncmp(name, "curve", NS_MIN(name_len, 5)) == 0)
        {
          curve = PL_strndup(value, value_len);
        }
        else if (PL_strncmp(name, "popcert", NS_MIN(name_len, 7)) == 0)
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
      return nsnull;

    PQGParams *pqgParams = nsnull;
    PQGVerify *vfy = nsnull;
    SECStatus  rv;
    int        index;
       
    index = PQG_PBITS_TO_INDEX(keySize);
    if (index == -1) {
      returnParams = nsnull;
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
      return nsnull;
    }
    returnParams = pqgParams;
    break;
  }
  default:
    returnParams = nsnull;
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
  PRUint32 mechanism = cryptojs_convert_to_mechanism(keyGenType);
  PK11SlotInfo *slot = nsnull;
  nsresult rv = GetSlotWithMechanism(mechanism,ctx, &slot);
  if (NS_FAILED(rv)) {
    if (slot)
      PK11_FreeSlot(slot);
    slot = nsnull;
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
                            PRInt32 keySize, char *params, 
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

  PRUint32 mechanism = cryptojs_convert_to_mechanism(keyPairInfo->keyGenType);
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
  PK11SlotInfo *intSlot = nsnull;
  PK11SlotInfoCleaner siCleaner(intSlot);
  
  if (willEscrow && !PK11_IsInternal(slot)) {
    intSlot = PK11_GetInternalSlot();
    NS_ASSERTION(intSlot,"Couldn't get the internal slot");
    
    if (!PK11_DoesMechanism(intSlot, mechanism)) {
      // Set to null, and the subsequent code will not attempt to use it.
      PK11_FreeSlot(intSlot);
      intSlot = nsnull;
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
  
  PK11SlotInfo *firstAttemptSlot = NULL;
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
    secondAttemptSlot = NULL;
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
        PK11SlotInfo *used_slot = NULL;
        rv = KeygenRunnable->ConsumeResult(&used_slot, 
                                           &keyPairInfo->privKey, &keyPairInfo->pubKey);

        if (NS_SUCCEEDED(rv)) {
          if ((used_slot == firstAttemptSlot) && (secondAttemptSlot != NULL)) {
            mustMoveKey = true;
          }
        
          PK11_FreeSlot(used_slot);
        }
      }
    }
  }

  firstAttemptSlot = NULL;
  secondAttemptSlot = NULL;
  
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
    SECKEYPrivateKey *newPrivKey = PK11_LoadPrivKey(slot, 
                                                    keyPairInfo->privKey,
                                                    keyPairInfo->pubKey,
                                                    true, true);
    SECKEYPrivateKeyCleaner pkCleaner(newPrivKey);

    if (!newPrivKey)
      return NS_ERROR_FAILURE;

    // The private key is stored on the selected slot now, and the copy we
    // ultimately use for escrowing when the time comes lives 
    // in the internal slot.  We will delete it from that slot
    // after the requests are made.  This call only gives up
    // our reference to the key object and does not actually 
    // physically remove it from the card itself.
    // The actual delete calls are being made in the destructors
    // of the cleaner helper instances.
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
 *    The PKCS11 slot to use for generating the key pair. If nsnull, then
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
                                jsval *argv,
                                nsKeyPairInfo *keyGenType,
                                nsIInterfaceRequestor *uiCxt,
                                PK11SlotInfo **slot, bool willEscrow)
{
  JSString  *jsString;
  JSAutoByteString params, keyGenAlg;
  int    keySize;
  nsresult  rv;

  if (!JSVAL_IS_INT(argv[0])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR,
                   "passed in non-integer for key size");
    return NS_ERROR_FAILURE;
  }
  keySize = JSVAL_TO_INT(argv[0]);
  if (!JSVAL_IS_NULL(argv[1])) {
    jsString = JS_ValueToString(cx,argv[1]);
    NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
    argv[1] = STRING_TO_JSVAL(jsString);
    params.encode(cx, jsString);
    NS_ENSURE_TRUE(!!params, NS_ERROR_OUT_OF_MEMORY);
  }

  if (JSVAL_IS_NULL(argv[2])) {
    JS_ReportError(cx,"%s%s\n", JS_ERROR,
             "key generation type not specified");
    return NS_ERROR_FAILURE;
  }
  jsString = JS_ValueToString(cx, argv[2]);
  NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
  argv[2] = STRING_TO_JSVAL(jsString);
  keyGenAlg.encode(cx, jsString);
  NS_ENSURE_TRUE(!!keyGenAlg, NS_ERROR_OUT_OF_MEMORY);
  keyGenType->keyGenType = cryptojs_interpret_key_gen_type(keyGenAlg.ptr());
  if (keyGenType->keyGenType == invalidKeyGen) {
    JS_ReportError(cx, "%s%s%s", JS_ERROR,
                   "invalid key generation argument:",
                   keyGenAlg.ptr());
    goto loser;
  }
  if (*slot == nsnull) {
    *slot = nsGetSlotForKeyGen(keyGenType->keyGenType, uiCxt);
    if (*slot == nsnull)
      goto loser;
  }

  rv = cryptojs_generateOneKeyPair(cx,keyGenType,keySize,params.ptr(),uiCxt,
                                   *slot,willEscrow);

  if (rv != NS_OK) {
    JS_ReportError(cx,"%s%s%s", JS_ERROR,
                   "could not generate the key for algorithm ",
                   keyGenAlg.ptr());
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
nsFreeCertReqMessages(CRMFCertReqMsg **certReqMsgs, PRInt32 numMessages)
{
  PRInt32 i;
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
  CERTCertificate *cert = wrappingCert->GetCert();
  if (!cert)
    return NS_ERROR_FAILURE;

  CRMFEncryptedKey *encrKey = 
      CRMF_CreateEncryptedKeyWithEncryptedValue(keyInfo->privKey, cert);
  CERT_DestroyCertificate(cert);
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
  CERTName *subjectName = CERT_AsciiToName(reqDN);
  if (!subjectName) {
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = CRMF_CertRequestSetTemplateField(certReq, crmfSubject,
                                                   static_cast<void*>
                                                              (subjectName));
  CERT_DestroyName(subjectName);
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
    SECItem *derEncoded = SEC_ASN1EncodeItem(nsnull, nsnull, &src, 
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
    SECItem *derEncoded = SEC_ASN1EncodeItem(nsnull, nsnull, &src,
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
  SECItem                 *encodedExt= nsnull;
  SECItem                  keyUsageValue = { (SECItemType) 0, nsnull, 0 };
  SECItem                  bitsmap = { (SECItemType) 0, nsnull, 0 };
  SECStatus                srv;
  CRMFCertExtension       *ext = nsnull;
  CRMFCertExtCreationInfo  extAddParams;
  SEC_ASN1Template         bitStrTemplate = {SEC_ASN1_BIT_STRING, 0, nsnull,
                                             sizeof(SECItem)};

  keyUsageValue.data = &keyUsage;
  keyUsageValue.len  = 1;
  nsPrepareBitStringForEncoding(&bitsmap, &keyUsageValue);

  encodedExt = SEC_ASN1EncodeItem(nsnull, nsnull, &bitsmap,&bitStrTemplate);
  if (encodedExt == nsnull) {
    goto loser;
  }
  ext = CRMF_CreateCertExtension(SEC_OID_X509_KEY_USAGE, true, encodedExt);
  if (ext == nsnull) {
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
  PRUint32 reqID;
  nsresult rv;

  //The draft says the ID of the request should be a random
  //number.  We don't have a way of tracking this number
  //to compare when the reply actually comes back,though.
  PK11_GenerateRandom((unsigned char*)&reqID, sizeof(reqID));
  CRMFCertRequest *certReq = CRMF_CreateCertRequest(reqID);
  if (!certReq)
    return nsnull;

  long version = SEC_CERTIFICATE_VERSION_3;
  SECStatus srv;
  CERTSubjectPublicKeyInfo *spki = nsnull;
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
  return nsnull;
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
                                              crmfChallengeResp, nsnull);
  }
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

static void PR_CALLBACK
nsCRMFEncoderItemCount(void *arg, const char *buf, unsigned long len);

static void PR_CALLBACK
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
  SECItem *der_request = NULL;
  SECItemCleanerTrueParam der_request_cleaner(der_request);

  if (SECSuccess != CRMF_EncodeCertRequest(certReq, 
                                           nsCRMFEncoderItemCount, 
                                           &der_request_len))
    return NS_ERROR_FAILURE;

  der_request = SECITEM_AllocItem(nsnull, nsnull, der_request_len);
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

  PK11SymKey *shared_secret = NULL;
  PK11SymKeyCleaner shared_secret_cleaner(shared_secret);

  PK11SymKey *subject_and_secret = NULL;
  PK11SymKeyCleaner subject_and_secret_cleaner(subject_and_secret);

  PK11SymKey *subject_and_secret_and_issuer = NULL;
  PK11SymKeyCleaner subject_and_secret_and_issuer_cleaner(subject_and_secret_and_issuer);

  PK11SymKey *sha1_of_subject_and_secret_and_issuer = NULL;
  PK11SymKeyCleaner sha1_of_subject_and_secret_and_issuer_cleaner(sha1_of_subject_and_secret_and_issuer);

  shared_secret = 
    PK11_PubDeriveWithKDF(keyInfo->privKey, // SECKEYPrivateKey *privKey
                          keyInfo->ecPopPubKey,  // SECKEYPublicKey *pubKey
                          false, // bool isSender
                          NULL, // SECItem *randomA
                          NULL, // SECItem *randomB
                          CKM_ECDH1_DERIVE, // CK_MECHANISM_TYPE derive
                          CKM_CONCATENATE_DATA_AND_BASE, // CK_MECHANISM_TYPE target
                          CKA_DERIVE, // CK_ATTRIBUTE_TYPE operation
                          0, // int keySize
                          CKD_NULL, // CK_ULONG kdf
                          NULL, // SECItem *sharedData
                          NULL); // void *wincx

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
                NULL, // SECItem *param
                CKM_SHA_1_HMAC, // CK_MECHANISM_TYPE target
                CKA_SIGN, // CK_ATTRIBUTE_TYPE operation
                0); // int keySize

  if (!sha1_of_subject_and_secret_and_issuer)
    return NS_ERROR_FAILURE;

  PK11Context *context = NULL;
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

  SECItem *result_hmac_sha1_item = NULL;
  SECItemCleanerTrueParam result_hmac_sha1_item_cleaner(result_hmac_sha1_item);

  result_hmac_sha1_item = SECITEM_AllocItem(nsnull, nsnull, SHA1_LENGTH);
  if (!result_hmac_sha1_item)
    return NS_ERROR_FAILURE;

  if (SECSuccess !=
      PK11_DigestFinal(context, 
                       result_hmac_sha1_item->data, 
                       &result_hmac_sha1_item->len, 
                       SHA1_LENGTH))
    return NS_ERROR_FAILURE;

  if (SECSuccess !=
      CRMF_CertReqMsgSetKeyAgreementPOP(certReqMsg, crmfDHMAC,
                                        crmfNoSubseqMess, result_hmac_sha1_item))
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
                                                 keyInfo->pubKey, nsnull,
                                                 nsnull, nsnull);

  if (srv == SECSuccess)
    return NS_OK;
  
  if (!gotDHMACParameters)
    return NS_ERROR_FAILURE;
  
  return nsSet_EC_DHMAC_ProofOfPossession(certReqMsg, keyInfo, certReq);
}

static void PR_CALLBACK
nsCRMFEncoderItemCount(void *arg, const char *buf, unsigned long len)
{
  unsigned long *count = (unsigned long *)arg;
  *count += len;
}

static void PR_CALLBACK
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
    return nsnull;
  }
  SECItem *dest = (SECItem *)PORT_Alloc(sizeof(SECItem));
  if (dest == nsnull) {
    return nsnull;
  }
  dest->type = siBuffer;
  dest->data = (unsigned char *)PORT_Alloc(len);
  if (dest->data == nsnull) {
    PORT_Free(dest);
    return nsnull;
  }
  dest->len = 0;

  if (CRMF_EncodeCertReqMessages(certReqMsgs, nsCRMFEncoderItemStore, dest)
      != SECSuccess) {
    SECITEM_FreeItem(dest, true);
    return nsnull;
  }
  return dest;
}

//Create a Base64 encoded CRMFCertReqMsg that can be sent to a CA
//requesting one or more certificates to be issued.  This function
//creates a single cert request per key pair and then appends it to
//a message that is ultimately sent off to a CA.
static char*
nsCreateReqFromKeyPairs(nsKeyPairInfo *keyids, PRInt32 numRequests,
                        char *reqDN, char *regToken, char *authenticator,
                        nsNSSCertificate *wrappingCert) 
{
  // We'use the goto notation for clean-up purposes in this function
  // that calls the C API of NSS.
  PRInt32 i;
  // The ASN1 encoder in NSS wants the last entry in the array to be
  // NULL so that it knows when the last element is.
  CRMFCertReqMsg **certReqMsgs = new CRMFCertReqMsg*[numRequests+1];
  CRMFCertRequest *certReq;
  if (!certReqMsgs)
    return nsnull;
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

  retString = NSSBase64_EncodeItem (nsnull, nsnull, 0, encodedReq);
  SECITEM_FreeItem(encodedReq, true);
  return retString;
loser:
  nsFreeCertReqMessages(certReqMsgs,numRequests);
  return nsnull;;
}

static nsISupports *
GetISupportsFromContext(JSContext *cx)
{
    if (JS_GetOptions(cx) & JSOPTION_PRIVATE_IS_NSISUPPORTS)
        return static_cast<nsISupports *>(JS_GetContextPrivate(cx));

    return nsnull;
}

//The top level method which is a member of nsIDOMCrypto
//for generate a base64 encoded CRMF request.
NS_IMETHODIMP
nsCrypto::GenerateCRMFRequest(nsIDOMCRMFObject** aReturn)
{
  nsNSSShutDownPreventionLock locker;
  *aReturn = nsnull;
  nsresult nrv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &nrv));
  NS_ENSURE_SUCCESS(nrv, nrv);

  nsAXPCNativeCallContext *ncc = nsnull;

  nrv = xpc->GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(nrv, nrv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  jsval *argv = nsnull;

  nrv = ncc->GetArgvPtr(&argv);
  NS_ENSURE_SUCCESS(nrv, nrv);

  JSContext *cx;

  nrv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(nrv, nrv);

  JSObject* script_obj = nsnull;
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  JSAutoRequest ar(cx);

  /*
   * Get all of the parameters.
   */
  if (argc < 5 || ((argc-5) % 3) != 0) {
    JS_ReportError(cx, "%s", "%s%s\n", JS_ERROR,
                  "incorrect number of parameters");
    return NS_ERROR_FAILURE;
  }
  
  if (JSVAL_IS_NULL(argv[0])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no DN specified");
    return NS_ERROR_FAILURE;
  }
  
  JSString *jsString = JS_ValueToString(cx,argv[0]);
  NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
  argv[0] = STRING_TO_JSVAL(jsString);
  JSAutoByteString reqDN(cx,jsString);
  NS_ENSURE_TRUE(!!reqDN, NS_ERROR_OUT_OF_MEMORY);

  JSAutoByteString regToken;
  if (!JSVAL_IS_NULL(argv[1])) {
    jsString = JS_ValueToString(cx, argv[1]);
    NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
    argv[1] = STRING_TO_JSVAL(jsString);
    regToken.encode(cx, jsString);
    NS_ENSURE_TRUE(!!regToken, NS_ERROR_OUT_OF_MEMORY);
  }
  JSAutoByteString authenticator;
  if (!JSVAL_IS_NULL(argv[2])) {
    jsString      = JS_ValueToString(cx, argv[2]);
    NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
    argv[2] = STRING_TO_JSVAL(jsString);
    authenticator.encode(cx, jsString);
    NS_ENSURE_TRUE(!!authenticator, NS_ERROR_OUT_OF_MEMORY);
  }
  JSAutoByteString eaCert;
  if (!JSVAL_IS_NULL(argv[3])) {
    jsString     = JS_ValueToString(cx, argv[3]);
    NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
    argv[3] = STRING_TO_JSVAL(jsString);
    eaCert.encode(cx, jsString);
    NS_ENSURE_TRUE(!!eaCert, NS_ERROR_OUT_OF_MEMORY);
  }
  if (JSVAL_IS_NULL(argv[4])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no completion "
                   "function specified");
    return NS_ERROR_FAILURE;
  }
  jsString = JS_ValueToString(cx, argv[4]);
  NS_ENSURE_TRUE(jsString, NS_ERROR_OUT_OF_MEMORY);
  argv[4] = STRING_TO_JSVAL(jsString);
  JSAutoByteString jsCallback(cx, jsString);
  NS_ENSURE_TRUE(!!jsCallback, NS_ERROR_OUT_OF_MEMORY);

  nrv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx),
                        static_cast<nsIDOMCrypto *>(this),
                        NS_GET_IID(nsIDOMCrypto), getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(nrv, nrv);

  nrv = holder->GetJSObject(&script_obj);
  NS_ENSURE_SUCCESS(nrv, nrv);

  //Put up some UI warning that someone is trying to 
  //escrow the private key.
  //Don't addref this copy.  That way ths reference goes away
  //at the same the nsIX09Cert ref goes away.
  nsNSSCertificate *escrowCert = nsnull;
  nsCOMPtr<nsIX509Cert> nssCert;
  bool willEscrow = false;
  if (!!eaCert) {
    SECItem certDer = {siBuffer, nsnull, 0};
    SECStatus srv = ATOB_ConvertAsciiToItem(&certDer, eaCert.ptr());
    if (srv != SECSuccess) {
      return NS_ERROR_FAILURE;
    }
    CERTCertificate *cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                    &certDer, nsnull, false,
                                                    true);
    if (!cert)
      return NS_ERROR_FAILURE;

    escrowCert = nsNSSCertificate::Create(cert);
    CERT_DestroyCertificate(cert);
    nssCert = escrowCert;
    if (!nssCert)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIDOMCryptoDialogs> dialogs;
    nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                                NS_GET_IID(nsIDOMCryptoDialogs),
                                NS_DOMCRYPTODIALOGS_CONTRACTID);
    if (NS_FAILED(rv))
      return rv;

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
    if (!okay)
      return NS_OK;
    willEscrow = true;
  }
  nsCOMPtr<nsIInterfaceRequestor> uiCxt = new PipUIContext;
  PRInt32 numRequests = (argc - 5)/3;
  nsKeyPairInfo *keyids = new nsKeyPairInfo[numRequests];
  if (keyids == nsnull) {
    JS_ReportError(cx, "%s\n", JS_ERROR_INTERNAL);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(keyids, 0, sizeof(nsKeyPairInfo)*numRequests);
  int keyInfoIndex;
  PRUint32 i;
  PK11SlotInfo *slot = nsnull;
  // Go through all of the arguments and generate the appropriate key pairs.
  for (i=5,keyInfoIndex=0; i<argc; i+=3,keyInfoIndex++) {
    nrv = cryptojs_ReadArgsAndGenerateKey(cx, &argv[i], &keyids[keyInfoIndex],
                                         uiCxt, &slot, willEscrow);
                                       
    if (NS_FAILED(nrv)) {
      if (slot)
        PK11_FreeSlot(slot);
      nsFreeKeyPairInfo(keyids,numRequests);
      return nrv;
    }
  }
  // By this time we'd better have a slot for the key gen.
  NS_ASSERTION(slot, "There was no slot selected for key generation");
  if (slot) 
    PK11_FreeSlot(slot);

  char *encodedRequest = nsCreateReqFromKeyPairs(keyids,numRequests,
                                                 reqDN.ptr(),regToken.ptr(),
                                                 authenticator.ptr(),
                                                 escrowCert);
#ifdef DEBUG_javi
  printf ("Created the folloing CRMF request:\n%s\n", encodedRequest);
#endif
  if (!encodedRequest) {
    nsFreeKeyPairInfo(keyids, numRequests);
    return NS_ERROR_FAILURE;
  }                                                    
  nsCRMFObject *newObject = new nsCRMFObject();
  if (newObject == nsnull) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "could not create crmf JS object");

    nsFreeKeyPairInfo(keyids,numRequests);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  newObject->SetCRMFRequest(encodedRequest);
  *aReturn = newObject;
  //Give a reference to the returnee.
  NS_ADDREF(*aReturn);
  nsFreeKeyPairInfo(keyids, numRequests);

  // 
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
  NS_ENSURE_TRUE(secMan, NS_ERROR_UNEXPECTED);
  
  nsCOMPtr<nsIPrincipal> principals;
  nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(principals));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(principals, NS_ERROR_UNEXPECTED);
  
  nsCryptoRunArgs *args = new nsCryptoRunArgs();
  if (!args)
    return NS_ERROR_OUT_OF_MEMORY;

  args->m_cx         = cx;
  args->m_kungFuDeathGrip = GetISupportsFromContext(cx);
  args->m_scope      = JS_GetParent(script_obj);

  args->m_jsCallback.Adopt(!!jsCallback ? nsCRT::strdup(jsCallback.ptr()) : 0);
  args->m_principals = principals;
  
  nsCryptoRunnable *cryptoRunnable = new nsCryptoRunnable(args);
  if (!cryptoRunnable)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = NS_DispatchToMainThread(cryptoRunnable);
  if (NS_FAILED(rv))
    delete cryptoRunnable;

  return rv;
}


// Reminder that we inherit the memory passed into us here.
// An implementation to let us back up certs as an event.
nsP12Runnable::nsP12Runnable(nsIX509Cert **certArr, PRInt32 numCerts,
                             nsIPK11Token *token)
{
  mCertArr  = certArr;
  mNumCerts = numCerts;
  mToken = token;
}

nsP12Runnable::~nsP12Runnable()
{
  PRInt32 i;
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

  PRInt16 dialogReturn;
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
  NS_ASSERTION(args,"Passed nsnull to nsCryptoRunnable constructor.");
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
  JSContext *cx = m_args->m_cx;

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;

  if (!ac.enter(cx, m_args->m_scope)) {
    return NS_ERROR_FAILURE;
  }

  // make sure the right context is on the stack. must not return w/out popping
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if (!stack || NS_FAILED(stack->Push(cx))) {
    return NS_ERROR_FAILURE;
  }

  JSBool ok =
    JS_EvaluateScriptForPrincipals(cx, m_args->m_scope,
                                   nsJSPrincipals::get(m_args->m_principals),
                                   m_args->m_jsCallback, 
                                   strlen(m_args->m_jsCallback),
                                   nsnull, 0, nsnull);
  stack->Pop(nsnull);
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

//Quick helper function to check if a newly issued cert
//already exists in the user's database.
static bool
nsCertAlreadyExists(SECItem *derCert)
{
  CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
  CERTCertificate *cert;
  bool retVal = false;

  cert = CERT_FindCertByDERCert(handle, derCert);
  if (cert) {
    if (cert->isperm && !cert->nickname && !cert->emailAddr) {
      //If the cert doesn't have a nickname or email addr, it is
      //bogus cruft, so delete it.
      SEC_DeletePermCertificate(cert);
    } else if (cert->isperm) {
      retVal = true;
    }
    CERT_DestroyCertificate(cert);
  }
  return retVal;
}

static PRInt32
nsCertListCount(CERTCertList *certList)
{
  PRInt32 numCerts = 0;
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
NS_IMETHODIMP
nsCrypto::ImportUserCertificates(const nsAString& aNickname, 
                                 const nsAString& aCmmfResponse, 
                                 bool aDoForcedBackup, 
                                 nsAString& aReturn)
{
  nsNSSShutDownPreventionLock locker;
  char *nickname=nsnull, *cmmfResponse=nsnull;
  CMMFCertRepContent *certRepContent = nsnull;
  int numResponses = 0;
  nsIX509Cert **certArr = nsnull;
  int i;
  CMMFCertResponse *currResponse;
  CMMFPKIStatus reqStatus;
  CERTCertificate *currCert;
  PK11SlotInfo *slot;
  nsCAutoString localNick;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = NS_OK;
  CERTCertList *caPubs = nsnull;
  nsCOMPtr<nsIPK11Token> token;

  nickname = ToNewCString(aNickname);
  cmmfResponse = ToNewCString(aCmmfResponse);
  if (nsCRT::strcmp("null", nickname) == 0) {
    nsMemory::Free(nickname);
    nickname = nsnull;
  }

  SECItem cmmfDer = {siBuffer, nsnull, 0};
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
    else if (nickname == nsnull || nickname[0] == '\0') {
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
    if (slot == nsnull) {
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
  caPubs = CMMF_CertRepContentGetCAPubs(certRepContent);
  if (caPubs) {
    PRInt32 numCAs = nsCertListCount(caPubs);
    
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
    
    CERT_DestroyCertList(caPubs);
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
    certArr = nsnull;

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
  return rv;
}

NS_IMETHODIMP
nsCrypto::PopChallengeResponse(const nsAString& aChallenge, 
                               nsAString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCrypto::Random(PRInt32 aNumBytes, nsAString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMETHODIMP
nsCrypto::SignText(const nsAString& aStringToSign, const nsAString& aCaOption,
                   nsAString& aResult)
{
  // XXX This code should return error codes, but we're keeping this
  //     backwards compatible with NS4.x and so we can't throw exceptions.
  NS_NAMED_LITERAL_STRING(internalError, "error:internalError");

  aResult.Truncate();

  nsAXPCNativeCallContext* ncc = nsnull;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  if (xpc) {
    xpc->GetCurrentNativeCallContext(&ncc);
  }

  if (!ncc) {
    aResult.Append(internalError);

    return NS_OK;
  }

  PRUint32 argc;
  ncc->GetArgc(&argc);

  JSContext *cx;
  ncc->GetJSContext(&cx);
  if (!cx) {
    aResult.Append(internalError);

    return NS_OK;
  }

  if (!aCaOption.EqualsLiteral("auto") &&
      !aCaOption.EqualsLiteral("ask")) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "caOption argument must be ask or auto");

    aResult.Append(internalError);

    return NS_OK;
  }

  // It was decided to always behave as if "ask" were specified.
  // XXX Should we warn in the JS Console for auto?

  nsCOMPtr<nsIInterfaceRequestor> uiContext = new PipUIContext;
  if (!uiContext) {
    aResult.Append(internalError);

    return NS_OK;
  }

  bool bestOnly = true;
  bool validOnly = true;
  CERTCertList* certList =
    CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), certUsageEmailSigner,
                              bestOnly, validOnly, uiContext);

  PRUint32 numCAs = argc - 2;
  if (numCAs > 0) {
    jsval *argv = nsnull;
    ncc->GetArgvPtr(&argv);

    nsAutoArrayPtr<JSAutoByteString> caNameBytes(new JSAutoByteString[numCAs]);
    if (!caNameBytes) {
      aResult.Append(internalError);
      return NS_OK;
    }

    JSAutoRequest ar(cx);

    PRUint32 i;
    for (i = 2; i < argc; ++i) {
      JSString *caName = JS_ValueToString(cx, argv[i]);
      NS_ENSURE_TRUE(caName, NS_ERROR_OUT_OF_MEMORY);
      argv[i] = STRING_TO_JSVAL(caName);
      caNameBytes[i - 2].encode(cx, caName);
      NS_ENSURE_TRUE(!!caNameBytes[i - 2], NS_ERROR_OUT_OF_MEMORY);
    }

    nsAutoArrayPtr<char*> caNames(new char*[numCAs]);
    if (!caNames) {
      aResult.Append(internalError);
      return NS_OK;
    }

    for (i = 0; i < numCAs; ++i)
      caNames[i] = caNameBytes[i].ptr();

    if (certList &&
        CERT_FilterCertListByCANames(certList, numCAs, caNames,
                                     certUsageEmailSigner) != SECSuccess) {
      aResult.Append(internalError);

      return NS_OK;
    }
  }

  if (!certList || CERT_LIST_EMPTY(certList)) {
    aResult.AppendLiteral("error:noMatchingCert");

    return NS_OK;
  }

  nsCOMPtr<nsIFormSigningDialog> fsd =
    do_CreateInstance(NS_FORMSIGNINGDIALOG_CONTRACTID);
  if (!fsd) {
    aResult.Append(internalError);

    return NS_OK;
  }

  nsCOMPtr<nsIDocument> document;
  GetDocumentFromContext(cx, getter_AddRefs(document));
  if (!document) {
    aResult.Append(internalError);

    return NS_OK;
  }

  // Get the hostname from the URL of the document.
  nsIURI* uri = document->GetDocumentURI();
  if (!uri) {
    aResult.Append(internalError);

    return NS_OK;
  }

  nsresult rv;

  nsCString host;
  rv = uri->GetHost(host);
  if (NS_FAILED(rv)) {
    aResult.Append(internalError);

    return NS_OK;
  }

  PRInt32 numberOfCerts = 0;
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(certList); !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    ++numberOfCerts;
  }

  CERTCertNicknames* nicknames = getNSSCertNicknamesFromCertList(certList);

  if (!nicknames) {
    aResult.Append(internalError);

    return NS_OK;
  }

  CERTCertNicknamesCleaner cnc(nicknames);

  NS_ASSERTION(nicknames->numnicknames == numberOfCerts,
               "nicknames->numnicknames != numberOfCerts");

  nsAutoArrayPtr<PRUnichar*> certNicknameList(new PRUnichar*[nicknames->numnicknames * 2]);
  if (!certNicknameList) {
    aResult.Append(internalError);

    return NS_OK;
  }

  PRUnichar** certDetailsList = certNicknameList.get() + nicknames->numnicknames;

  PRInt32 certsToUse;
  for (node = CERT_LIST_HEAD(certList), certsToUse = 0;
       !CERT_LIST_END(node, certList) && certsToUse < nicknames->numnicknames;
       node = CERT_LIST_NEXT(node)) {
    nsRefPtr<nsNSSCertificate> tempCert = nsNSSCertificate::Create(node->cert);
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
    aResult.Append(internalError);

    return NS_OK;
  }

  NS_ConvertUTF8toUTF16 utf16Host(host);

  CERTCertificate *signingCert = nsnull;
  bool tryAgain, canceled;
  nsAutoString password;
  do {
    // Throw up the form signing confirmation dialog and get back the index
    // of the selected cert.
    PRInt32 selectedIndex = -1;
    rv = fsd->ConfirmSignText(uiContext, utf16Host, aStringToSign,
                              const_cast<const PRUnichar**>(certNicknameList.get()),
                              const_cast<const PRUnichar**>(certDetailsList),
                              certsToUse, &selectedIndex, password,
                              &canceled);
    if (NS_FAILED(rv) || canceled) {
      break; // out of tryAgain loop
    }

    PRInt32 j = 0;
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

  PRInt32 k;
  for (k = 0; k < certsToUse; ++k) {
    nsMemory::Free(certNicknameList[k]);
    nsMemory::Free(certDetailsList[k]);
  }

  if (NS_FAILED(rv)) { // something went wrong inside the tryAgain loop
    aResult.Append(internalError);

    return NS_OK;
  }

  if (canceled) {
    aResult.AppendLiteral("error:userCancel");

    return NS_OK;
  }

  SECKEYPrivateKey* privKey = PK11_FindKeyByAnyCert(signingCert, uiContext);
  if (!privKey) {
    aResult.Append(internalError);

    return NS_OK;
  }

  nsCAutoString charset(document->GetDocumentCharacterSet());

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
        aResult.Append(internalError);

        return NS_OK;
      }
    }
    else {
      AppendUTF16toUTF8(aStringToSign, buffer);
    }
  }

  HASHContext *hc = HASH_Create(HASH_AlgSHA1);
  if (!hc) {
    aResult.Append(internalError);

    return NS_OK;
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
                                                       nsnull, SEC_OID_SHA1,
                                                       &digest, nsnull, uiContext);
  if (ci) {
    srv = SEC_PKCS7IncludeCertChain(ci, nsnull);
    if (srv == SECSuccess) {
      srv = SEC_PKCS7AddSigningTime(ci);
      if (srv == SECSuccess) {
        srv = SEC_PKCS7Encode(ci, signTextOutputCallback, &p7, nsnull, nsnull,
                              uiContext);
      }
    }

    SEC_PKCS7DestroyContentInfo(ci);
  }

  if (srv != SECSuccess) {
    aResult.Append(internalError);

    return NS_OK;
  }

  SECItem binary_item;
  binary_item.data = reinterpret_cast<unsigned char*>
                                     (const_cast<char*>(p7.get()));
  binary_item.len = p7.Length();

  char *result = NSSBase64_EncodeItem(nsnull, nsnull, 0, &binary_item);
  if (result) {
    AppendASCIItoUTF16(result, aResult);
  }
  else {
    aResult.Append(internalError);
  }

  PORT_Free(result);

  return NS_OK;
}

//Logout out of all installed PKCS11 tokens.
NS_IMETHODIMP
nsCrypto::Logout()
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  {
    nsNSSShutDownPreventionLock locker;
    PK11_LogoutAll();
    SSL_ClearSessionCache();
  }

  return nssComponent->LogoutAuthenticatedPK11();
}

NS_IMETHODIMP
nsCrypto::DisableRightClick()
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

nsPkcs11::nsPkcs11()
{
}

nsPkcs11::~nsPkcs11()
{
}

//Quick function to confirm with the user.
bool
confirm_user(const PRUnichar *message)
{
  PRInt32 buttonPressed = 1; // If the user exits by clicking the close box, assume No (button 1)

  nsCOMPtr<nsIPrompt> prompter;
  (void) nsNSSComponent::GetNewPrompter(getter_AddRefs(prompter));

  if (prompter) {
    nsPSMUITracker tracker;
    if (!tracker.isUIForbidden()) {
      // The actual value is irrelevant but we shouldn't be handing out
      // malformed JSBools to XPConnect.
      bool checkState = false;
      prompter->ConfirmEx(0, message,
                          (nsIPrompt::BUTTON_DELAY_ENABLE) +
                          (nsIPrompt::BUTTON_POS_1_DEFAULT) +
                          (nsIPrompt::BUTTON_TITLE_OK * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_CANCEL * nsIPrompt::BUTTON_POS_1),
                          nsnull, nsnull, nsnull, nsnull, &checkState, &buttonPressed);
    }
  }

  return (buttonPressed == 0);
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
  
  char *modName = ToNewCString(aModuleName);
  PRInt32 modType;
  SECStatus srv = SECMOD_DeleteModule(modName, &modType);
  if (srv == SECSuccess) {
    SECMODModule *module = SECMOD_FindModule(modName);
    if (module) {
      nssComponent->ShutdownSmartCardThread(module);
      SECMOD_DestroyModule(module);
    }
    rv = NS_OK;
  } else {
    rv = NS_ERROR_FAILURE;
  }
  NS_Free(modName);
  return rv;
}

//Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
nsPkcs11::AddModule(const nsAString& aModuleName, 
                    const nsAString& aLibraryFullPath, 
                    PRInt32 aCryptoMechanismFlags, 
                    PRInt32 aCipherFlags)
{
  nsNSSShutDownPreventionLock locker;
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  char *moduleName = ToNewCString(aModuleName);
  char *fullPath   = ToNewCString(aLibraryFullPath);
  PRUint32 mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  PRUint32 cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleName, fullPath, 
                                      mechFlags, cipherFlags);
  if (srv == SECSuccess) {
    SECMODModule *module = SECMOD_FindModule(moduleName);
    if (module) {
      nssComponent->LaunchSmartCardThread(module);
      SECMOD_DestroyModule(module);
    }
  }

  nsMemory::Free(moduleName);
  nsMemory::Free(fullPath);

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

