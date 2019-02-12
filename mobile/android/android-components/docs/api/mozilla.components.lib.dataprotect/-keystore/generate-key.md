[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [generateKey](./generate-key.md)

# generateKey

`fun generateKey(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L173)

Generates the managed key if it does not already exist.

### Exceptions

`GeneralSecurityException` - If the key could not be created

**Return**
`true` if a new key was generated; `false` if the key already exists and can
be used.

