[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [decryptBytes](./decrypt-bytes.md)

# decryptBytes

`open fun decryptBytes(encrypted: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L235)

Decrypts data using the managed key.

The input of this method is expected to include input factors (i.e., initialization
vector), ciphertext, and authentication tag as a single byte string; it is the direct
output from [encryptBytes](encrypt-bytes.md).

### Parameters

`encrypted` - The encrypted data to decrypt

### Exceptions

`KeystoreException` - If the data could not be decrypted

**Return**
The decrypted "plaintext" data

