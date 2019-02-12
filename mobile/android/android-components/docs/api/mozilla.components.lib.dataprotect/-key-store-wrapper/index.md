[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](./index.md)

# KeyStoreWrapper

`open class KeyStoreWrapper` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L38)

Wraps the critical functions around a Java KeyStore to better facilitate testing
and instrumenting.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `KeyStoreWrapper()`<br>Wraps the critical functions around a Java KeyStore to better facilitate testing and instrumenting. |

### Functions

| Name | Summary |
|---|---|
| [getKeyFor](get-key-for.md) | `open fun getKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Key`](https://developer.android.com/reference/java/security/Key.html)`?`<br>Retrieves the SecretKey for the given label. |
| [getKeyStore](get-key-store.md) | `fun getKeyStore(): `[`KeyStore`](https://developer.android.com/reference/java/security/KeyStore.html)<br>Retrieves the underlying KeyStore, loading it if necessary. |
| [loadKeyStore](load-key-store.md) | `open fun loadKeyStore(): `[`KeyStore`](https://developer.android.com/reference/java/security/KeyStore.html)<br>Creates and initializes the KeyStore in use. |
| [makeKeyFor](make-key-for.md) | `open fun makeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SecretKey`](https://developer.android.com/reference/javax/crypto/SecretKey.html)<br>Creates a SecretKey for the given label. |
| [removeKeyFor](remove-key-for.md) | `fun removeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes a key with the given label. |
