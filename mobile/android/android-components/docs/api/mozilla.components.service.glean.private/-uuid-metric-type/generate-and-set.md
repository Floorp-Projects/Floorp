[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [UuidMetricType](index.md) / [generateAndSet](./generate-and-set.md)

# generateAndSet

`fun generateAndSet(): `[`UUID`](https://developer.android.com/reference/java/util/UUID.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/UuidMetricType.kt#L37)

Generate a new UUID value and set it in the metric store.

**Return**
a [UUID](https://developer.android.com/reference/java/util/UUID.html) or [null](#) if we're not allowed to record.

