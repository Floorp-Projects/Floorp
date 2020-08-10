[android-components](../index.md) / [mozilla.components.lib.dataprotect](./index.md)

## Package mozilla.components.lib.dataprotect

### Types

| Name | Summary |
|---|---|
| [KeyStoreWrapper](-key-store-wrapper/index.md) | `open class KeyStoreWrapper`<br>Wraps the critical functions around a Java KeyStore to better facilitate testing and instrumenting. |
| [Keystore](-keystore/index.md) | `open class Keystore`<br>Manages data protection using a system-isolated cryptographic key. |
| [SecureAbove22Preferences](-secure-above22-preferences/index.md) | `class SecureAbove22Preferences : KeyValuePreferences`<br>A wrapper around [SharedPreferences](#) which encrypts contents on supported API versions (23+). Otherwise, this simply delegates to [SharedPreferences](#). |

### Exceptions

| Name | Summary |
|---|---|
| [KeystoreException](-keystore-exception/index.md) | `class KeystoreException : `[`GeneralSecurityException`](http://docs.oracle.com/javase/7/docs/api/java/security/GeneralSecurityException.html)<br>Exception type thrown by {@link Keystore} when an error is encountered that is not otherwise covered by an existing sub-class to `GeneralSecurityException`. |

### Functions

| Name | Summary |
|---|---|
| [generateEncryptionKey](generate-encryption-key.md) | `fun generateEncryptionKey(keyStrength: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Generates a random key of specified [keyStrength](generate-encryption-key.md#mozilla.components.lib.dataprotect$generateEncryptionKey(kotlin.Int)/keyStrength). |
