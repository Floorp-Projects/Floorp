[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [KeyStoreWrapper](index.md) / [removeKeyFor](./remove-key-for.md)

# removeKeyFor

`fun removeKeyFor(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L102)

Deletes a key with the given label.

### Parameters

`label` - The label of the associated key to delete

### Exceptions

`KeyStoreException` - If there is no key for `label`