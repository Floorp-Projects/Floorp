[android-components](../index.md) / [mozilla.components.lib.dataprotect](./index.md)

## Package mozilla.components.lib.dataprotect

### Types

| Name | Summary |
|---|---|
| [KeyStoreWrapper](-key-store-wrapper/index.md) | `open class KeyStoreWrapper`<br>Wraps the critical functions around a Java KeyStore to better facilitate testing and instrumenting. |
| [Keystore](-keystore/index.md) | `open class Keystore`<br>Manages data protection using a system-isolated cryptographic key. |

### Exceptions

| Name | Summary |
|---|---|
| [KeystoreException](-keystore-exception/index.md) | `class KeystoreException : `[`GeneralSecurityException`](https://developer.android.com/reference/java/security/GeneralSecurityException.html)<br>Exception type thrown by {@link Keystore} when an error is encountered that is not otherwise covered by an existing sub-class to `GeneralSecurityException`. |
