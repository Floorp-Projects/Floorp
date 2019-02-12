[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [encryptBytes](./encrypt-bytes.md)

# encryptBytes

`open fun encryptBytes(plain: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L212)

Encrypts data using the managed key.

The output of this method includes the input factors (i.e., initialization vector),
ciphertext, and authentication tag as a single byte string; this output can be passed
directly to [decryptBytes](decrypt-bytes.md).

### Parameters

`plain` - The "plaintext" data to encrypt

### Exceptions

`GeneralSecurityException` - If the data could not be encrypted

**Return**
The encrypted data to be stored

