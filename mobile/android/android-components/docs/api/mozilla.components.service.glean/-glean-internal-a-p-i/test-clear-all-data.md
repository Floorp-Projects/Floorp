[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [testClearAllData](./test-clear-all-data.md)

# testClearAllData

`fun testClearAllData(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L296)

Test only function to clear all storages and metrics.  Note that this also includes 'user'
lifetime metrics so be aware that things like clientId will be wiped as well.

