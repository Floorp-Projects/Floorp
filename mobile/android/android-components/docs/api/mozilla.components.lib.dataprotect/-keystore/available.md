[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [available](./available.md)

# available

`fun available(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L168)

Determines if the managed key is available for use.  Consumers can use this to
determine if the key was somehow lost and should treat any previously-protected
data as invalid.

**Return**
`true` if the managed key exists and ready for use.

