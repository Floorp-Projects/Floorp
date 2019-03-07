[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [setUploadEnabled](./set-upload-enabled.md)

# setUploadEnabled

`fun setUploadEnabled(enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L112)

Enable or disable glean collection and upload.

Metric collection is enabled by default.

When disabled, metrics aren't recorded at all and no data
is uploaded.

### Parameters

`enabled` - When true, enable metric collection.