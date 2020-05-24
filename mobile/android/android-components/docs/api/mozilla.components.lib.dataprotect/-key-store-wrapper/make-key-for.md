[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](index.md) / [makeKeyFor](./make-key-for.md)

# makeKeyFor

`open fun makeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SecretKey`](http://docs.oracle.com/javase/7/docs/api/javax/crypto/SecretKey.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L87)

Creates a SecretKey for the given label.

This method generates a SecretKey pre-bound to the `AndroidKeyStore` and configured
with the strongest "algorithm/blockmode/padding" (and key size) available.

Subclasses override this method to properly associate the generated key with
the given label in the underlying KeyStore.

### Parameters

`label` - The label to associate with the created key

### Exceptions

`NoSuchAlgorithmException` - If the cipher algorithm is not supported

**Return**
The newly-generated key for `label`

