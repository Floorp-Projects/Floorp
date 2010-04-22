/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM IWeaveCrypto.idl
 */

#ifndef __gen_IWeaveCrypto_h__
#define __gen_IWeaveCrypto_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    IWeaveCrypto */
#define IWEAVECRYPTO_IID_STR "f4463043-315e-41f3-b779-82e900e6fffa"

#define IWEAVECRYPTO_IID \
  {0xf4463043, 0x315e, 0x41f3, \
    { 0xb7, 0x79, 0x82, 0xe9, 0x00, 0xe6, 0xff, 0xfa }}

class NS_NO_VTABLE NS_SCRIPTABLE IWeaveCrypto : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(IWEAVECRYPTO_IID)

  /**
   * Shortcuts for some algorithm SEC OIDs.  Full list available here:
   * http://lxr.mozilla.org/seamonkey/source/security/nss/lib/util/secoidt.h
   */
  enum { DES_EDE3_CBC = 156U };

  enum { AES_128_CBC = 184U };

  enum { AES_192_CBC = 186U };

  enum { AES_256_CBC = 188U };

  /**
   * One of the above constants. Used as the mechanism for encrypting bulk
   * data and wrapping keys.
   *
   * Default is AES_256_CBC.
   */
  /* attribute unsigned long algorithm; */
  NS_SCRIPTABLE NS_IMETHOD GetAlgorithm(PRUint32 *aAlgorithm) = 0;
  NS_SCRIPTABLE NS_IMETHOD SetAlgorithm(PRUint32 aAlgorithm) = 0;

  /**
   * The size of the RSA key to create with generateKeypair().
   *
   * Default is 2048.
   */
  /* attribute unsigned long keypairBits; */
  NS_SCRIPTABLE NS_IMETHOD GetKeypairBits(PRUint32 *aKeypairBits) = 0;
  NS_SCRIPTABLE NS_IMETHOD SetKeypairBits(PRUint32 aKeypairBits) = 0;

  /**
   * Encrypt data using a symmetric key.
   * The algorithm attribute specifies how the encryption is performed.
   *
   * @param   clearText
   *          The data to be encrypted (not base64 encoded).
   * @param   symmetricKey
   *          A base64-encoded symmetric key (eg, one from generateRandomKey).
   * @param   iv
   *          A base64-encoded initialization vector
   * @returns Encrypted data, base64 encoded
   */
  /* ACString encrypt (in AUTF8String clearText, in ACString symmetricKey, in ACString iv); */
  NS_SCRIPTABLE NS_IMETHOD Encrypt(const nsACString & clearText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) = 0;

  /**
   * Encrypt data using a symmetric key.
   * The algorithm attribute specifies how the encryption is performed.
   *
   * @param   cipherText
   *          The base64-encoded data to be decrypted
   * @param   symmetricKey
   *          A base64-encoded symmetric key (eg, one from unwrapSymmetricKey)
   * @param   iv
   *          A base64-encoded initialization vector
   * @returns Decrypted data (not base64-encoded)
   */
  /* AUTF8String decrypt (in ACString cipherText, in ACString symmetricKey, in ACString iv); */
  NS_SCRIPTABLE NS_IMETHOD Decrypt(const nsACString & cipherText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) = 0;

  /**
   * Generate a RSA public/private keypair.
   *
   * @param aPassphrase
   *        User's passphrase. Used with PKCS#5 to generate a symmetric key
   *        for wrapping the private key.
   * @param aSalt
   *        Salt for the user's passphrase.
   * @param aIV
   *        Random IV, used when wrapping the private key.
   * @param aEncodedPublicKey
   *        The public key, base-64 encoded.
   * @param aWrappedPrivateKey
   *        The public key, encrypted with the user's passphrase, and base-64 encoded.
   */
  /* void generateKeypair (in ACString aPassphrase, in ACString aSalt, in ACString aIV, out ACString aEncodedPublicKey, out ACString aWrappedPrivateKey); */
  NS_SCRIPTABLE NS_IMETHOD GenerateKeypair(const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & aEncodedPublicKey NS_OUTPARAM, nsACString & aWrappedPrivateKey NS_OUTPARAM) = 0;

  /* ACString generateRandomKey (); */
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomKey(nsACString & _retval NS_OUTPARAM) = 0;

  /* ACString generateRandomIV (); */
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomIV(nsACString & _retval NS_OUTPARAM) = 0;

  /* ACString generateRandomBytes (in unsigned long aByteCount); */
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomBytes(PRUint32 aByteCount, nsACString & _retval NS_OUTPARAM) = 0;

  /**
   * Encrypts a symmetric key with a user's public key.
   *
   * @param aSymmetricKey
   *        The base64 encoded string holding a symmetric key.
   * @param aEncodedPublicKey
   *        The base64 encoded string holding a public key.
   * @returns The wrapped symmetric key, base64 encoded
   *
   * For RSA, the unencoded public key is a PKCS#1 object.
   */
  /* ACString wrapSymmetricKey (in ACString aSymmetricKey, in ACString aEncodedPublicKey); */
  NS_SCRIPTABLE NS_IMETHOD WrapSymmetricKey(const nsACString & aSymmetricKey, const nsACString & aEncodedPublicKey, nsACString & _retval NS_OUTPARAM) = 0;

  /**
   * Decrypts a symmetric key with a user's private key.
   *
   * @param aWrappedSymmetricKey
   *        The base64 encoded string holding an encrypted symmetric key.
   * @param aWrappedPrivateKey
   *        The base64 encoded string holdering an encrypted private key.
   * @param aPassphrase
   *        The passphrase to decrypt the private key.
   * @param aSalt
   *        The salt for the passphrase.
   * @param aIV
   *        The random IV used when unwrapping the private key.
   * @returns The unwrapped symmetric key, base64 encoded
   *
   * For RSA, the unencoded, decrypted key is a PKCS#1 object.
   */
  /* ACString unwrapSymmetricKey (in ACString aWrappedSymmetricKey, in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV); */
  NS_SCRIPTABLE NS_IMETHOD UnwrapSymmetricKey(const nsACString & aWrappedSymmetricKey, const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & _retval NS_OUTPARAM) = 0;

  /**
   * Rewrap a private key with a new user passphrase.
   *
   * @param aWrappedPrivateKey
   *        The base64 encoded string holding an encrypted private key.
   * @param aPassphrase
   *        The passphrase to decrypt the private key.
   * @param aSalt
   *        The salt for the passphrase.
   * @param aIV
   *        The random IV used when unwrapping the private key.
   * @param aNewPassphrase
   *        The new passphrase to wrap the private key with.
   * @returns The (re)wrapped private key, base64 encoded
   *
   */
  /* ACString rewrapPrivateKey (in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV, in ACString aNewPassphrase); */
  NS_SCRIPTABLE NS_IMETHOD RewrapPrivateKey(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, const nsACString & aNewPassphrase, nsACString & _retval NS_OUTPARAM) = 0;

  /**
    * Verify a user's passphrase against a private key.
    *
    * @param aWrappedPrivateKey
    *        The base64 encoded string holding an encrypted private key.
    * @param aPassphrase
    *        The passphrase to decrypt the private key.
    * @param aSalt
    *        The salt for the passphrase.
    * @param aIV
    *        The random IV used when unwrapping the private key.
    * @returns Boolean true if the passphrase decrypted the key correctly.
    *
    */
  /* boolean verifyPassphrase (in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV); */
  NS_SCRIPTABLE NS_IMETHOD VerifyPassphrase(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, PRBool *_retval NS_OUTPARAM) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(IWeaveCrypto, IWEAVECRYPTO_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_IWEAVECRYPTO \
  NS_SCRIPTABLE NS_IMETHOD GetAlgorithm(PRUint32 *aAlgorithm); \
  NS_SCRIPTABLE NS_IMETHOD SetAlgorithm(PRUint32 aAlgorithm); \
  NS_SCRIPTABLE NS_IMETHOD GetKeypairBits(PRUint32 *aKeypairBits); \
  NS_SCRIPTABLE NS_IMETHOD SetKeypairBits(PRUint32 aKeypairBits); \
  NS_SCRIPTABLE NS_IMETHOD Encrypt(const nsACString & clearText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD Decrypt(const nsACString & cipherText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GenerateKeypair(const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & aEncodedPublicKey NS_OUTPARAM, nsACString & aWrappedPrivateKey NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomKey(nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomIV(nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomBytes(PRUint32 aByteCount, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD WrapSymmetricKey(const nsACString & aSymmetricKey, const nsACString & aEncodedPublicKey, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD UnwrapSymmetricKey(const nsACString & aWrappedSymmetricKey, const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD RewrapPrivateKey(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, const nsACString & aNewPassphrase, nsACString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD VerifyPassphrase(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, PRBool *_retval NS_OUTPARAM); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_IWEAVECRYPTO(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetAlgorithm(PRUint32 *aAlgorithm) { return _to GetAlgorithm(aAlgorithm); } \
  NS_SCRIPTABLE NS_IMETHOD SetAlgorithm(PRUint32 aAlgorithm) { return _to SetAlgorithm(aAlgorithm); } \
  NS_SCRIPTABLE NS_IMETHOD GetKeypairBits(PRUint32 *aKeypairBits) { return _to GetKeypairBits(aKeypairBits); } \
  NS_SCRIPTABLE NS_IMETHOD SetKeypairBits(PRUint32 aKeypairBits) { return _to SetKeypairBits(aKeypairBits); } \
  NS_SCRIPTABLE NS_IMETHOD Encrypt(const nsACString & clearText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) { return _to Encrypt(clearText, symmetricKey, iv, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD Decrypt(const nsACString & cipherText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) { return _to Decrypt(cipherText, symmetricKey, iv, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateKeypair(const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & aEncodedPublicKey NS_OUTPARAM, nsACString & aWrappedPrivateKey NS_OUTPARAM) { return _to GenerateKeypair(aPassphrase, aSalt, aIV, aEncodedPublicKey, aWrappedPrivateKey); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomKey(nsACString & _retval NS_OUTPARAM) { return _to GenerateRandomKey(_retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomIV(nsACString & _retval NS_OUTPARAM) { return _to GenerateRandomIV(_retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomBytes(PRUint32 aByteCount, nsACString & _retval NS_OUTPARAM) { return _to GenerateRandomBytes(aByteCount, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD WrapSymmetricKey(const nsACString & aSymmetricKey, const nsACString & aEncodedPublicKey, nsACString & _retval NS_OUTPARAM) { return _to WrapSymmetricKey(aSymmetricKey, aEncodedPublicKey, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD UnwrapSymmetricKey(const nsACString & aWrappedSymmetricKey, const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & _retval NS_OUTPARAM) { return _to UnwrapSymmetricKey(aWrappedSymmetricKey, aWrappedPrivateKey, aPassphrase, aSalt, aIV, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD RewrapPrivateKey(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, const nsACString & aNewPassphrase, nsACString & _retval NS_OUTPARAM) { return _to RewrapPrivateKey(aWrappedPrivateKey, aPassphrase, aSalt, aIV, aNewPassphrase, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD VerifyPassphrase(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, PRBool *_retval NS_OUTPARAM) { return _to VerifyPassphrase(aWrappedPrivateKey, aPassphrase, aSalt, aIV, _retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_IWEAVECRYPTO(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetAlgorithm(PRUint32 *aAlgorithm) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetAlgorithm(aAlgorithm); } \
  NS_SCRIPTABLE NS_IMETHOD SetAlgorithm(PRUint32 aAlgorithm) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetAlgorithm(aAlgorithm); } \
  NS_SCRIPTABLE NS_IMETHOD GetKeypairBits(PRUint32 *aKeypairBits) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetKeypairBits(aKeypairBits); } \
  NS_SCRIPTABLE NS_IMETHOD SetKeypairBits(PRUint32 aKeypairBits) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetKeypairBits(aKeypairBits); } \
  NS_SCRIPTABLE NS_IMETHOD Encrypt(const nsACString & clearText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->Encrypt(clearText, symmetricKey, iv, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD Decrypt(const nsACString & cipherText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->Decrypt(cipherText, symmetricKey, iv, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateKeypair(const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & aEncodedPublicKey NS_OUTPARAM, nsACString & aWrappedPrivateKey NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->GenerateKeypair(aPassphrase, aSalt, aIV, aEncodedPublicKey, aWrappedPrivateKey); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomKey(nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->GenerateRandomKey(_retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomIV(nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->GenerateRandomIV(_retval); } \
  NS_SCRIPTABLE NS_IMETHOD GenerateRandomBytes(PRUint32 aByteCount, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->GenerateRandomBytes(aByteCount, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD WrapSymmetricKey(const nsACString & aSymmetricKey, const nsACString & aEncodedPublicKey, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->WrapSymmetricKey(aSymmetricKey, aEncodedPublicKey, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD UnwrapSymmetricKey(const nsACString & aWrappedSymmetricKey, const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->UnwrapSymmetricKey(aWrappedSymmetricKey, aWrappedPrivateKey, aPassphrase, aSalt, aIV, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD RewrapPrivateKey(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, const nsACString & aNewPassphrase, nsACString & _retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->RewrapPrivateKey(aWrappedPrivateKey, aPassphrase, aSalt, aIV, aNewPassphrase, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD VerifyPassphrase(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, PRBool *_retval NS_OUTPARAM) { return !_to ? NS_ERROR_NULL_POINTER : _to->VerifyPassphrase(aWrappedPrivateKey, aPassphrase, aSalt, aIV, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public IWeaveCrypto
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IWEAVECRYPTO

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, IWeaveCrypto)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* attribute unsigned long algorithm; */
NS_IMETHODIMP _MYCLASS_::GetAlgorithm(PRUint32 *aAlgorithm)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP _MYCLASS_::SetAlgorithm(PRUint32 aAlgorithm)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute unsigned long keypairBits; */
NS_IMETHODIMP _MYCLASS_::GetKeypairBits(PRUint32 *aKeypairBits)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP _MYCLASS_::SetKeypairBits(PRUint32 aKeypairBits)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString encrypt (in AUTF8String clearText, in ACString symmetricKey, in ACString iv); */
NS_IMETHODIMP _MYCLASS_::Encrypt(const nsACString & clearText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* AUTF8String decrypt (in ACString cipherText, in ACString symmetricKey, in ACString iv); */
NS_IMETHODIMP _MYCLASS_::Decrypt(const nsACString & cipherText, const nsACString & symmetricKey, const nsACString & iv, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void generateKeypair (in ACString aPassphrase, in ACString aSalt, in ACString aIV, out ACString aEncodedPublicKey, out ACString aWrappedPrivateKey); */
NS_IMETHODIMP _MYCLASS_::GenerateKeypair(const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & aEncodedPublicKey NS_OUTPARAM, nsACString & aWrappedPrivateKey NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString generateRandomKey (); */
NS_IMETHODIMP _MYCLASS_::GenerateRandomKey(nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString generateRandomIV (); */
NS_IMETHODIMP _MYCLASS_::GenerateRandomIV(nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString generateRandomBytes (in unsigned long aByteCount); */
NS_IMETHODIMP _MYCLASS_::GenerateRandomBytes(PRUint32 aByteCount, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString wrapSymmetricKey (in ACString aSymmetricKey, in ACString aEncodedPublicKey); */
NS_IMETHODIMP _MYCLASS_::WrapSymmetricKey(const nsACString & aSymmetricKey, const nsACString & aEncodedPublicKey, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString unwrapSymmetricKey (in ACString aWrappedSymmetricKey, in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV); */
NS_IMETHODIMP _MYCLASS_::UnwrapSymmetricKey(const nsACString & aWrappedSymmetricKey, const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* ACString rewrapPrivateKey (in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV, in ACString aNewPassphrase); */
NS_IMETHODIMP _MYCLASS_::RewrapPrivateKey(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, const nsACString & aNewPassphrase, nsACString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean verifyPassphrase (in ACString aWrappedPrivateKey, in ACString aPassphrase, in ACString aSalt, in ACString aIV); */
NS_IMETHODIMP _MYCLASS_::VerifyPassphrase(const nsACString & aWrappedPrivateKey, const nsACString & aPassphrase, const nsACString & aSalt, const nsACString & aIV, PRBool *_retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_IWeaveCrypto_h__ */
