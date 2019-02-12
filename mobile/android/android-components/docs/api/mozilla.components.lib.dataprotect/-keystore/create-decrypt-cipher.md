[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [createDecryptCipher](./create-decrypt-cipher.md)

# createDecryptCipher

`open fun createDecryptCipher(iv: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`Cipher`](https://developer.android.com/reference/javax/crypto/Cipher.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L284)

Create a cipher initialized for decrypting data with the managed key.

This "low-level" method is useful when a cryptographic context is needed to integrate with
other APIs, such as the `FingerprintManager`.

**NOTE:** The caller is responsible for associating certain encryption factors, such as
the initialization vector and/or additional authentication data (AAD), with the stored
ciphertext or decryption will fail.

### Parameters

`iv` - The initialization vector/nonce to decrypt with

### Exceptions

`GeneralSecurityException` - If the cipher could not be created and initialized

**Return**
The [Cipher](https://developer.android.com/reference/javax/crypto/Cipher.html), initialized and ready to decrypt data with.

