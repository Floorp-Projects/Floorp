.. _mozilla_projects_nss_nss_tech_notes_nss_tech_note5:

nss tech note5
==============

.. _using_nss_to_perform_miscellaneous_cryptographic_operations:

`Using NSS to perform miscellaneous cryptographic operations <#using_nss_to_perform_miscellaneous_cryptographic_operations>`__
------------------------------------------------------------------------------------------------------------------------------

.. container::

.. _nss_technical_note_5:

`NSS Technical Note: 5 <#nss_technical_note_5>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   -  NSS Project Info is at
      `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__
   -  You can browse the NSS source online at http://lxr.mozilla.org/mozilla/source/security/nss/
       and http://lxr.mozilla.org/security/
   -  Be sure to look for :ref:`mozilla_projects_nss_sample_code` first for things you need to do.
   -  **Note:** This document contains code snippets that focus on essential aspects of the task and
      often do not illustrate all the cleanup that needs to be done. Also, this document does not
      attempt to be an exhaustive survey of all possible ways to do a certain task; it merely tries
      to show a certain way.

   --------------

`Encrypt/Decrypt <#encryptdecrypt>`__
-------------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.The simplest Init function, in case you don't need a NSS database
      is
      *NSS_NoDB_Init(".")*
   #. Choose a cipher mechanism. Note that some mechanisms (*_PAD) imply the padding is handled for
      you by NSS. If you choose something else, then data padding is the application's
      responsibility. You can find a list of cipher mechanisms in security/nss/lib/softoken/pkcs11.c
      - grep for CKF_EN_DE_.
      *CK_MECHANISM_TYPE cipherMech = CKM_DES_CBC_PAD* <big>(for example)</big>
   #. Choose a slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(cipherMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* alwys returns internal slot, may not be
      optimal \*/*
   #. Prepare the Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* SymKey = PK11_ImportSymKey(slot, cipherMech, PK11_OriginUnwrap,
                                                                                                
         CKA_ENCRYPT, &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__
          

   #. <big>Prepare the parameter for crypto context. IV is relevant only when using CBC mode of
      encryption. If not using CBC mode, just pass a NULL IV parm to PK11_ParamFromIV function
      *SECItem ivItem;
      ivItem.data = /\* ptr to an array of IV bytes \*/
      ivItem.len = /\* length of the array of IV bytes \*/
      SECItem \*SecParam = PK11_ParamFromIV(cipherMech, &ivItem);*\ </big>
   #. <big>Now encrypt and decrypt using the key and parameter setup in above steps</big>

      -  Create Encryption context
         *PK11Context\* EncContext = PK11_CreateContextBySymKey(cipherMech,
                                                                                        
          CKA_ENCRYPT or CKA_DECRYPT,
                                                                                          SymKey,
         SecParam);*
      -  Do the Operation. If encrypting, outbuf len must be atleast (inbuflen + blocksize). If
         decrypting, outbuflen must be atleast inbuflen.
         *SECStatus s = PK11_CipherOp(EncContext, outbuf, &tmp1_outlen, sizeof outbuf, inbuf,
                                                                     inbuflen);
         s = PK11_DigestFinal(EncContext, outbuf+tmp1_outlen, &tmp2_outlen,
                                                    sizeof outbuf - tmp1_outlen);
         result_len = tmp1_outlen + tmp2_outlen;*
      -  <big>Destroy the Context
         *PK11_DestroyContext(EncContext, PR_TRUE);*\ </big>

   #. <big>Repeat Step 6 **any number of times**. When all done with encrypt/decrypt ops, clean
      up</big>
      <big>\ *PK11_FreeSymKey(SymKey);
      SECITEM_FreeItem(SecParam, PR_TRUE);
      PK11_FreeSlot(slot);*\ </big>

   | **Note:** AES encryption, a fixed blocksize of 16 bytes is used. The Rijndael algorithm permits
     3 blocksizes (16, 24, 32 bytes), but the AES standard requires the blocksize to be 16 bytes.
     The keysize can vary and these keysizes are permitted: 16, 24, 32 bytes.
   | You can also look at a `sample program <../sample-code/sample2.html>`__ illustrating encryption

   --------------

.. _hash_digest:

`Hash / Digest <#hash_digest>`__
--------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.The simplest Init function, in case you don't need a NSS database
      is
      *NSS_NoDB_Init(".")*
   #. <big>Create Digest context</big>. Some of the digest algorithm identifiers are (without the
      SEC_OID\_ prefix) : MD2, MD5, SHA1, SHA256, SHA384, SHA512.
      *PK11Context\* DigestContext = PK11_CreateDigestContext(SEC_OID_MD5);*
   #. <big>Digest the data</big>
      <big>\ *SECStatus s = PK11_DigestBegin(DigestContext);
      s = PK11_DigestOp(DigestContext, data, sizeof data);
      s = PK11_DigestFinal(DigestContext, digest, &len, sizeof digest);
      /\* now, digest contains the 'digest', and len contains the length of the digest \*/*\ </big>
   #. Clean up
      *PK11_DestroyContext(DigestContext, PR_TRUE);*

   |
   | You can also look at a `sample program <../sample-code/sample3.html>`__ illustrating this

   --------------

.. _hash_digest_with_secret_key_included:

`Hash / Digest with secret key included <#hash_digest_with_secret_key_included>`__
----------------------------------------------------------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.The simplest Init function, in case you don't need a NSS database
      is
      *NSS_NoDB_Init(".")*
   #. Choose a digest mechanism. You can find a list of digest mechanisms in
      security/nss/lib/softoken/pkcs11.c - grep for CKF_DIGEST.
      *CK_MECHANISM_TYPE digestMech = CKM_MD5* <big>(for example)</big>
   #. Choose a slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(digestMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* always returns int slot, may not be
      optimal \*/*
   #. Prepare the Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* SymKey = PK11_ImportSymKey(slot, digestMech, PK11_OriginUnwrap,
                                                                                                
         CKA_DIGEST, &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__. Can use *CKM_GENERIC_SECRET_KEY_GEN* as the key gen
         mechanism.
          

   #. <big>Prepare the parameter for crypto context. The param must be provided, but can be empty.
      *SECItem param;
      param.data = 0;
      param.len = 0;*\ </big>
   #. <big>Create Crypto context</big>
      *PK11Context\* DigestContext = PK11_CreateContextBySymKey(digestMech, CKA_DIGEST, SymKey,
                                                                                                   
                               &param);*
   #. <big>Digest the data</big>, providing the key
      <big>\ *SECStatus s = PK11_DigestBegin(DigestContext);
      s = PK11_DigestKey(DigestContext, SymKey);
      s = PK11_DigestOp(DigestContext, data, sizeof data);
      s = PK11_DigestFinal(DigestContext, digest, &len, sizeof digest);
      /\* now, digest contains the 'digest', and len contains the length of the digest \*/*\ </big>
   #. Clean up
      *PK11_DestroyContext(DigestContext, PR_TRUE);
      PK11_FreeSymKey(SymKey);
      PK11_FreeSlot(slot);*

   You can also look at a `sample program <../sample-code/sample3.html>`__ illustrating this

   --------------

`HMAC <#hmac>`__
----------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.The simplest Init function, in case you don't need a NSS database
      is
      *NSS_NoDB_Init(".")*
   #. Choose a  HMAC mechanism. You can find a list of HMAC mechanisms in
      security/nss/lib/softoken/pkcs11.c - grep for CKF_SN_VR, and choose the mechanisms that
      contain HMAC in the name
      *CK_MECHANISM_TYPE hmacMech = CKM_MD5_HMAC;* <big>(for example)</big>
   #. Choose a slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(hmacMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* always returns int slot, may not be
      optimal \*/*
   #. Prepare the Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.type = siBuffer;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* SymKey = PK11_ImportSymKey(slot, hmacMech, PK11_OriginUnwrap,
                                                                                                
         CKA_SIGN,  &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__. Can use *CKM_GENERIC_SECRET_KEY_GEN* as the key gen
         mechanism.
          

   #. <big>Prepare the parameter for crypto context. The param must be provided, but can be empty.
      *SECItem param;
      param.type = siBuffer;
      param.data = NULL;
      param.len = 0;*\ </big>
   #. <big>Create Crypto context</big>
      *PK11Context\* DigestContext = PK11_CreateContextBySymKey(hmacMech, CKA_SIGN,
                                                                                                   
                               SymKey, &param);*
   #. <big>Digest the data</big>
      <big>\ *SECStatus s = PK11_DigestBegin(DigestContext);
      s = PK11_DigestOp(DigestContext, data, sizeof data);
      s = PK11_DigestFinal(DigestContext, digest, &len, sizeof digest);
      /\* now, digest contains the 'signed digest', and len contains the length of the digest
      \*/*\ </big>
   #. Clean up
      *PK11_DestroyContext(DigestContext, PR_TRUE);*
      *PK11_FreeSymKey(SymKey);
      PK11_FreeSlot(slot);*

   |
   | You can also look at a `sample program <../sample-code/sample3.html>`__ illustrating this

   --------------

.. _symmetric_key_wrappingunwrapping_of_a_symmetric_key:

`Symmetric Key Wrapping/Unwrapping of a Symmetric Key <#symmetric_key_wrappingunwrapping_of_a_symmetric_key>`__
---------------------------------------------------------------------------------------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.The simplest Init function, in case you don't need a NSS database
      is
      *NSS_NoDB_Init(".")*
   #. Choose a  Wrapping mechanism. See wrapMechanismList in security/nss/lib/pk11wrap/pk11slot.c
      and security/nss/lib/ssl/ssl3con.c for examples of wrapping mechanisms. Most of them are
      cipher mechanisms.
      *CK_MECHANISM_TYPE wrapMech = CKM_DES3_ECB;* <big>(for example)</big>
   #. Choose a slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(wrapMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* always returns int slot, may not be
      optimal \*/*
      <big>Regarding the choice of slot and wrapMech, if you know one, you can derive the other. You
      can get the best slot given a wrap mechanism (as shown above), or get the best wrap mechanism
      given a slot using:</big>
      *CK_MECHANISM_TYPE wrapMech = PK11_GetBestWrapMechanism(slot)*
   #. Prepare the Wrapping Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* WrappingSymKey = PK11_ImportSymKey(slot, wrapMech,
                                                                                                    
                      PK11_OriginUnwrap,
                                                                                                    
                      CKA_WRAP,  &keyItem, NULL)*
          
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__
          

   #. Prepare the To-be-Wrapped Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* ToBeWrappedSymKey = PK11_ImportSymKey(slot, wrapMech,,
                                                                                                    
                              PK11_OriginUnwrap,
                                                                                                    
                             CKA_WRAP,  &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__
          

   #. <big>Prepare the parameter for crypto context. IV is relevant only when using CBC cipher mode.
      If not using CBC mode, just pass a NULL *SecParam* to *PK11_WrapSymKey* or *PK11_UnwrapSymKey*
      function
      *SECItem ivItem;
      ivItem.data = /\* ptr to an array of IV bytes \*/
      ivItem.len = /\* length of the array of IV bytes \*/
      SECItem \*SecParam = PK11_ParamFromIV(wrapMech, &ivItem);*\ </big>
   #. Allocate space for the wrapped key
      *SECItem WrappedKey;
      WrappedKey.len = SOME_LEN;
      WrappedKey.data = allocate (SOME_LEN) bytes;*
   #. <big>Do the Wrap</big>. Note that the WrappingSymKey and the ToBeWrappedSymKey must be on the
      slot where the wrap is going to happen. To move  keys to the desired slot, see section `Moving
      a Key from one slot to another <#moving_a_key_from_one_slot_to_another>`__
      <big>\ *SECStatus s = PK11_WrapSymKey(wrapMech, SecParam, WrappingSymKey,
                                                                       ToBeWrappedSymKey,
      &WrappedKey);*\ </big>
   #. <big><big>Transport/Store or do whatever with the Wrapped Key (WrappedKey.data,
      WrappedKey.len)</big></big>
   #. <big><big>Unwrapping. </big></big>

      -  <big><big>Set up the args to the function *PK11_UnwrapSymKey*, most of which are
         illustrated above. The *keyTypeMech* arg of type *CK_MECHANISM_TYPE  *\ <big>indicates the
         type of key that was wrapped and can be same as the *wrapMech* (e.g.
         *wrapMech=CKM_SKIPJACK_WRAP, keyTypeMech=CKM_SKIPJACK_CBC64; wrapMech=CKM_SKIPJACK_CBC64,
         keyTypeMech=CKM_SKIPJACK_CBC64*).</big>\ </big></big>
      -  Do the unwrap
         <big><big>\ *PK11SymKey\* UnwrappedSymKey = PK11_UnwrapSymKey(WrappingSymKey,
                                                                                            
         wrapMech*\ </big></big><big><big>\ *, SecParam, &WrappedKey,
                                                                                            
         keyTypeMech,*\ </big></big>
         <big><big>\ *                                                                             
              CKA_UNWRAP, /\* or CKA_DECRYPT? \*/
                                                                                          
          size_of_key_that_was_wrapped_bytes);*\ </big></big>

   #. Clean up
      *PK11_FreeSymKey(WrappingSymKey);*
      *PK11_FreeSymKey(ToBeWrappedSymKey);
      PK11_FreeSymKey(UnwrappedSymKey);
      if (SecParam) SECITEM_FreeItem(SecParam, PR_TRUE);
      SECITEM_FreeItem(&WrappedKey, PR_TRUE);
      PK11_FreeSlot(slot); *

   --------------

.. _symmetric_key_wrappingunwrapping_of_a_private_key:

`Symmetric Key Wrapping/Unwrapping of a Private Key <#symmetric_key_wrappingunwrapping_of_a_private_key>`__
-----------------------------------------------------------------------------------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.
   #. Choose a  Wrapping mechanism. See wrapMechanismList in security/nss/lib/pk11wrap/pk11slot.c
      and security/nss/lib/ssl/ssl3con.c for examples of wrapping mechanisms. Most of them are
      cipher mechanisms.
      *CK_MECHANISM_TYPE wrapMech = CKM_DES3_ECB;* <big>(for example).</big>
   #. Slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(wrapMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* always returns int slot, may not be
      optimal \*/*
      This should be the slot that is best suited for the wrapping. This may or may not be the slot
      that contains the private key or the slot that contains the Symmetric key.
      <big>Regarding the choice of slot and wrapMech, if you know one, you can derive the other. You
      can get the best slot given a wrap mechanism (as shown above), or get the best wrap mechanism
      given a slot using:</big>
      *CK_MECHANISM_TYPE wrapMech = PK11_GetBestWrapMechanism(slot)*
   #. Prepare the Wrapping Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* WrappingSymKey = PK11_ImportSymKey(slot, wrapMech,
                                                                                                    
                      PK11_OriginUnwrap,
                                                                                                    
                      CKA_WRAP,  &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__
          

   #. Prepare the To-be-Wrapped Key

      -  *SECKEYPrivateKey \*ToBeWrappedPrivKey *

   #. <big>Prepare the parameter for crypto context. IV is relevant only when using CBC cipher mode.
      If not using CBC mode, just pass a NULL *SecParam* to *PK11_WrapPrivKey* function
      *SECItem ivItem;
      ivItem.data = /\* ptr to an array of IV bytes \*/
      ivItem.len = /\* length of the array of IV bytes \*/
      SECItem \*SecParam = PK11_ParamFromIV(wrapMech, &ivItem);*\ </big>
   #. Allocate space for the wrapped key. Note that a 2048-bit *wrapped* RSA private key takes up
      around 1200 bytes.
      *SECItem WrappedKey;
      WrappedKey.len = SOME_LEN;
      WrappedKey.data = allocate (SOME_LEN) bytes;*
   #. <big>Do the Wrap</big>. Note that the WrappingSymKey and the ToBeWrappedPvtKey must be on the
      slot where the wrap is going to happen. To move  keys to the desired slot, see section `Moving
      a Key from one slot to another <#moving_a_key_from_one_slot_to_another>`__
      <big>\ *SECStatus s = PK11_WrapPrivKey(slot, WrappingSymKey,  ToBeWrappedPvtKey, wrapMech,
                                                                       SecParam, &WrappedKey,
      NULL);*\ </big>
   #. <big><big>Transport/Store or do whatever with the Wrapped Key (WrappedKey.data,
      WrappedKey.len)</big></big>
   #. <big><big>Unwrapping.</big></big>

      -  Prepare the args for the unwrap function. Most of the args are illustrated above
         *SECItem label; /\* empty, doesn't need to be freed \*/
         label.data = NULL; label.len = 0;*
         *SECItem \*pubValue = NULL;
         pubValue = /\* ?? \*/;*
         *PRBool token = /\* PR_TRUE or PR_FALSE depending on?? \*/
         CK_MECHANISM_TYPE keyTypeMech = ??;
         CK_KEY_TYPE keyType;
         keyType = PK11_GetKeyType(keyTypeMech, 0);
         CK_ATTRIBUTE_TYPE attribs[4];
         int numAttribs;
         /\* figure out which operations to enable for this key \*/
         if( keyType == CKK_RSA ) {
                 attribs[0] = CKA_SIGN;
                 attribs[1] = CKA_DECRYPT;
                 attribs[2] = CKA_SIGN_RECOVER;
                 attribs[3] = CKA_UNWRAP;
                 numAttribs = 4;
         } else if(keyType == CKK_DSA) {
                 attribs[0] = CKA_SIGN;
                 numAttribs = 1;
         }*
      -  <big>Do the unwrap</big>
         *SECKEYPrivateKey \*UnwrappedPvtKey =
                       PK11_UnwrapPrivKey(slot, WrappingSymKey, wrapMech, SecParam, &WrappedKey,
                                                                   &label,  pubValue, token, PR_TRUE
         /\* sensitive \*/
                                                                   keyType,  attribs, numAttribs,
         NULL /*wincx*/);*

   #. Clean up
      *PK11_FreeSymKey(WrappingSymKey);*
      <big>\ *if (SecParam) SECITEM_FreeItem(SecParam, PR_TRUE);*\ </big>
      <big>\ *SECITEM_FreeItem(&WrappedKey, PR_TRUE);*\ </big>
      *if (pubValue)  SECITEM_FreeItem(pubValue, PR_TRUE);*
      *if (UnwrappedPvtKey) SECKEY_DestroyPrivateKey(UnwrappedPvtKey);*
      *if (ToBeWrappedPvtKey) SECKEY_DestroyPrivateKey(ToBeWrappedPvtKey);*
      *PK11_FreeSlot(slot);*

   --------------

.. _public_key_wrapping_private_key_unwrapping_of_a_symmetric_key_(pki_based_key_transport):

`Public Key Wrapping & Private Key Unwrapping of a Symmetric Key (PKI based key transport) <#public_key_wrapping_private_key_unwrapping_of_a_symmetric_key_(pki_based_key_transport)>`__
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

.. container::

   #. Include headers
      *#include "nss.h"
      #include "pk11pub.h"*
   #. Make sure NSS is initialized.
   #. Choose a  Wrapping mechanism. See wrapMechanismList in security/nss/lib/pk11wrap/pk11slot.c
      and security/nss/lib/ssl/ssl3con.c for examples of wrapping mechanisms. Most of them are
      cipher mechanisms.
      *CK_MECHANISM_TYPE wrapMech = CKM_DES3_ECB;* <big>(for example)</big>
   #. Slot on which to do the operation
      *PK11SlotInfo\* slot = PK11_GetBestSlot(wrapMech, NULL);  *\ **OR**\ *
      PK11SlotInfo\* slot = PK11_GetInternalKeySlot(); /\* always returns int slot, may not be
      optimal \*/*
      This should be the slot that is best suited for the wrapping. This may or may not be the slot
      that contains the public/private key or the slot that contains the Symmetric key.
      <big>Regarding the choice of slot and wrapMech, if you know one, you can derive the other. You
      can get the best slot given a wrap mechanism (as shown above), or get the best wrap mechanism
      given a slot using:</big>
      *CK_MECHANISM_TYPE wrapMech = PK11_GetBestWrapMechanism(slot)*
   #. Prepare the Wrapping Key

      -  *SECKeyPublicKey \*WrappingPubKey*

   #. Prepare the To-be-Wrapped Key

      -  If using a raw key
         */\* turn the raw key into a SECItem \*/
         SECItem keyItem;
         keyItem.data = /\* ptr to an array of key bytes \*/
         keyItem.len = /\* length of the array of key bytes \*/
         /\* turn the SECItem into a key object \*/
         PK11SymKey\* ToBeWrappedSymKey = PK11_ImportSymKey(slot, wrapMech,,
                                                                                                    
                              PK11_OriginUnwrap,
                                                                                                    
                             CKA_WRAP,  &keyItem, NULL)*;
      -  If generating the key - see section `Generate a Symmetric
         Key <#generate_a_symmetric_key>`__

   #. Allocate space for the wrapped key
      *SECItem WrappedKey;
      WrappedKey.len = SOME_LEN;
      WrappedKey.data = allocate (SOME_LEN) bytes;*
   #. <big>Do the Wrap</big>. Note that the WrappingPubKey and the ToBeWrappedSymKey must be on the
      slot where the wrap is going to happen. To move  keys to the desired slot, see section `Moving
      a Key from one slot to another <#moving_a_key_from_one_slot_to_another>`__
      <big>\ *SECStatus s = PK11_PubWrapSymKey(wrapMech, WrappingPubKey,
                                                                              ToBeWrappedSymKey,
      &WrappedKey);*\ </big>
   #. <big><big>Transport/Store or do whatever with the Wrapped Key (WrappedKey.data,
      WrappedKey.len)</big></big>
   #. <big><big>Unwrapping. </big></big>

      -  Prepare the args for the unwrap function. Most of the args are illustrated above
         *SECKEYPrivateKey \*UnWrappingPvtKey;
         CK_MECHANISM_TYPE keyTypeMech = ??;*
      -  <big>Do the unwrap</big>
         *PK11SymKey \*UnwrappedSymKey =
                   PK11_PubUnwrapSymKey(UnWrappingPvtKey, WrappedKey, keyTypeMech,
                                                                      *<big><big>\ *CKA_UNWRAP, /\*
         or CKA_DECRYPT? \*/
                                                                    
          *\ </big></big><big><big>\ *size_of_key_that_was_wrapped_bytes);*\ </big></big>

   #. Clean up
      *PK11_FreeSymKey(ToBeWrappedSymKey);*
      <big>\ *SECITEM_FreeItem(&WrappedKey, PR_TRUE);*\ </big>
      *if (WrappingPubKey) SECKEY_DestroyPublicKey(WrappingPubKey);*
      *if (UnwrappingPvtKey) SECKEY_DestroyPrivateKey(UnwrappingPvtKey);*
      *PK11_FreeSlot(slot);*

   Also look at a `sample program <../sample-code/sample1.html>`__ that uses the above functions.

   --------------

.. _generate_a_symmetric_key_2:

`Generate a Symmetric Key <#generate_a_symmetric_key_2>`__
----------------------------------------------------------

.. container::

   | Subsequent to the operation, the symmetric key may need to be transported/stored in wrapped or
     raw form. You can find a list of key generation mechanisms in
     security/nss/lib/softoken/pkcs11.c - grep for CKF_GENERATE. For some key gen mechanisms, the
     keysize is in bytes, and for some it is in bits.
   |  

   #. <big>Choose a key generation mechanism</big>
      *CK_MECHANISM_TYPE keygenMech = CKM_DES_KEY_GEN;* (for example)
   #. <big>Generate the key</big>
      *PK11SymKey\* SymKey = PK11_KeyGen(slot, keygenMech, NULL, keysize, NULL);*

   <big>You can also see an `sample program <../sample-code/sample1.html>`__ that does key
   generation.</big>

   .. rubric:: Extract the raw key (This should not normally be used. Better to use wrapping
      instead. See `method1 <#symmetric_key_wrappingunwrapping_sym_key>`__ and
      `method2 <#pki_wrap_symkey>`__ ). 
      :name: extract_the_raw_key_(this_should_not_normally_be_used._better_to_use_wrapping_instead._see_method1_and_method2_).

   *SECStatus rv = PK11_ExtractKeyValue(SymKey);
   SECItem \*keydata = PK11_GetKeyData(SymKey);*

   .. rubric:: Generating a persistent symmetric key
      :name: generating_a_persistent_symmetric_key

   | *SECItem keyid;
     CK_MECHANISM_TYPE cipherMech = CKM_AES_CBC_PAD;
     keyid.data = /\* ptr to an array of bytes representing the id of the key to be generated \*/;
     keyid.len = /\* length of the array of bytes \*/;
     /\* keysize must be 0 for fixed key-length algorithms like DES... and appropriate value
      \*  for non fixed-key-length algorithms \*/
     PK11SymKey \*key = PK11_TokenKeyGen(slot, cipherMech, 0, 32 /\* keysize \*/,
                                                                                    &keyid, PR_TRUE,
     0);*
   | *int keylen = PK11_GetKeyLength(key);
     cipherMech = PK11_GetMechanism(key);*
   | */\* find the symmetric key in the database \*/
     key = PK11_FindFixedKey(slot, cipherMech, &keyid, 0);*

   --------------

.. _moving_a_key_from_one_slot_to_another_2:

`Moving a Key from one slot to another <#moving_a_key_from_one_slot_to_another_2>`__
------------------------------------------------------------------------------------

.. container::

   -  To move a Private key from one slot to another, wrap the private key on the origin slot and
      unwrap it into the destination slot. See section `Symmetric Key Wrapping/Unwrapping of a
      Private Key <#symmetric_key_wrappingunwrapping_pvtkey>`__
   -  To move a Symmetric key
      *PK11SymKey \*destSymKey = pk11_CopyToSlot(destslot, wrapMech, CKA_UNWRAP?, origSymKey);*

   --------------

.. _generate_an_rsa_key_pair:

`Generate an RSA Key Pair <#generate_an_rsa_key_pair>`__
--------------------------------------------------------

.. container::

   *PK11_GenerateKeyPair*\ <big> is the function to use</big>. See a `sample
   program <../sample-code/sample1.html>`__ that uses this function.

   --------------

.. _<big>sign_verify_data<big>:

`<big>Sign & Verify Data</big> <#%3Cbig%3Esign_verify_data%3Cbig%3E>`__
-----------------------------------------------------------------------

.. container::

   | *SECKEYPrivateKey \*pvtkey;
     SECItem signature;
     SECItem data;
     SECStatus s = PK11_Sign(pvtkey, &signature, &data);*
   | *SECKeyPublicKey \*pubkey;*
   | *SECStatus s = PK11_Verify(pubkey, &signature, &data, NULL);*

   --------------

.. _misc_useful_functions:

`Misc Useful Functions <#misc_useful_functions>`__
--------------------------------------------------

.. container::

   #. Get the best wrapping mechanism supported by a slot
      *CK_MECHANISM_TYPE mech = PK11_GetBestWrapMechanism(PK11SlotInfo \*slot);*
   #. <big>Get the best slot for a certain mechanism</big>
      *PK11SlotInfo\* slot = PK11_GetBestSlot(mechanism, NULL);*
   #. <big>Get the best key length for a certain mechanism on a given slot</big>
      *int keylen = PK11_GetBestKeyLength(PK11SlotInfo \*slot, mechanism);*
   #. Get the key length of a symmetric key
      *int keylen = PK11_GetKeyLength(PK11SymKey \*symkey);*
   #. Get the mechanism given a symmetric key
      *CK_MECHANISM_TYPE mech = PK11_GetMechanism(PK11SymKey \*key);*
       

   --------------