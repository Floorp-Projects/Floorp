[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](./index.md)

# KeyStoreWrapper

`open class KeyStoreWrapper` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L41)

Wraps the critical functions around a Java KeyStore to better facilitate testing
and instrumenting.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `KeyStoreWrapper()`<br>Wraps the critical functions around a Java KeyStore to better facilitate testing and instrumenting. |

### Functions

| Name | Summary |
|---|---|
| [getKeyFor](get-key-for.md) | `open fun getKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Key`](http://docs.oracle.com/javase/7/docs/api/java/security/Key.html)`?`<br>Retrieves the SecretKey for the given label. |
| [getKeyStore](get-key-store.md) | `fun getKeyStore(): `[`KeyStore`](http://docs.oracle.com/javase/7/docs/api/java/security/KeyStore.html)<br>Retrieves the underlying KeyStore, loading it if necessary. |
| [loadKeyStore](load-key-store.md) | `open fun loadKeyStore(): `[`KeyStore`](http://docs.oracle.com/javase/7/docs/api/java/security/KeyStore.html)<br>Creates and initializes the KeyStore in use. |
| [makeKeyFor](make-key-for.md) | `open fun makeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SecretKey`](http://docs.oracle.com/javase/7/docs/api/javax/crypto/SecretKey.html)<br>Creates a SecretKey for the given label. |
| [removeKeyFor](remove-key-for.md) | `fun removeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Deletes a key with the given label. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
