[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](index.md) / [getKeyFor](./get-key-for.md)

# getKeyFor

`open fun getKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Key`](https://developer.android.com/reference/java/security/Key.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L68)

Retrieves the SecretKey for the given label.

This method queries for a SecretKey with the given label and no passphrase.

Subclasses override this method if additional properties are needed
to retrieve the key.

### Parameters

`label` - The label to query

### Exceptions

`InvalidKeyException` - If there is a Key but it is not a SecretKey

`NoSuchAlgorithmException` - If the recovery algorithm is not supported

`UnrecoverableKeyException` - If the key could not be recovered for some reason

**Return**
The key for the given label, or `null` if not present

