[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](./index.md)

# Keystore

`open class Keystore` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L142)

Manages data protection using a system-isolated cryptographic key.

This class provides for both:

* management for a specific crypto graphic key (identified by a string label)
* protection (encryption/decryption) of data using the managed key

The specific cryptographic properties are pre-chosen to be the following:

* Algorithm is "AES/GCM/NoPadding"
* Key size is 256 bits
* Tag size is 128 bits

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Keystore(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, manual: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, wrapper: `[`KeyStoreWrapper`](../-key-store-wrapper/index.md)` = KeyStoreWrapper())`<br>Creates a new instance around a key identified by the given label |

### Properties

| Name | Summary |
|---|---|
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The label the cryptographic key is identified as |

### Functions

| Name | Summary |
|---|---|
| [available](available.md) | `fun available(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Determines if the managed key is available for use.  Consumers can use this to determine if the key was somehow lost and should treat any previously-protected data as invalid. |
| [createDecryptCipher](create-decrypt-cipher.md) | `open fun createDecryptCipher(iv: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`Cipher`](https://developer.android.com/reference/javax/crypto/Cipher.html)<br>Create a cipher initialized for decrypting data with the managed key. |
| [createEncryptCipher](create-encrypt-cipher.md) | `open fun createEncryptCipher(): `[`Cipher`](https://developer.android.com/reference/javax/crypto/Cipher.html)<br>Create a cipher initialized for encrypting data with the managed key. |
| [decryptBytes](decrypt-bytes.md) | `open fun decryptBytes(encrypted: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)<br>Decrypts data using the managed key. |
| [deleteKey](delete-key.md) | `fun deleteKey(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes the managed key. |
| [encryptBytes](encrypt-bytes.md) | `open fun encryptBytes(plain: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)<br>Encrypts data using the managed key. |
| [generateKey](generate-key.md) | `fun generateKey(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Generates the managed key if it does not already exist. |
