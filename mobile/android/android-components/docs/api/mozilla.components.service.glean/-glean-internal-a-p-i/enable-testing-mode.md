[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [enableTestingMode](./enable-testing-mode.md)

# enableTestingMode

`fun enableTestingMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L373)

Should be called from all users of the glean testing API.

This makes all asynchronous work synchronous so we can test the results of the
API synchronously.

