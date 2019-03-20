[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [CommonMetricData](index.md) / [defaultStorageDestinations](./default-storage-destinations.md)

# defaultStorageDestinations

`abstract val defaultStorageDestinations: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/CommonMetricData.kt#L64)

Defines the names of the storages the metric defaults to when
"default" is used as the destination storage.
Note that every metric type will need to override this.

