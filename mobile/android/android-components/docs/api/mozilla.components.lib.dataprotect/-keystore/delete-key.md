[android-components](../../index.md) / [mozilla.components.lib.dataprotect](../index.md) / [Keystore](index.md) / [deleteKey](./delete-key.md)

# deleteKey

`fun deleteKey(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/dataprotect/src/main/java/mozilla/components/lib/dataprotect/Keystore.kt#L193)

Deletes the managed key.

**NOTE:** Once this method returns, any data protected with the (formerly) managed
key cannot be decrypted and therefore is inaccessble.

