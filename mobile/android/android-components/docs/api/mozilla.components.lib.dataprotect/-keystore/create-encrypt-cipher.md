[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [createEncryptCipher](./create-encrypt-cipher.md)

# createEncryptCipher

`open fun createEncryptCipher(): `[`Cipher`](https://developer.android.com/reference/javax/crypto/Cipher.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L261)

Create a cipher initialized for encrypting data with the managed key.

This "low-level" method is useful when a cryptographic context is needed to integrate with
other APIs, such as the `FingerprintManager`.

**NOTE:** The caller is responsible for associating certain encryption factors, such as
the initialization vector and/or additional authentication data (AAD), with the resulting
ciphertext or decryption will fail.

### Exceptions

`GeneralSecurityException` - If the Cipher could not be created and initialized

**Return**
The [Cipher](https://developer.android.com/reference/javax/crypto/Cipher.html), initialized and ready to encrypt data with.

