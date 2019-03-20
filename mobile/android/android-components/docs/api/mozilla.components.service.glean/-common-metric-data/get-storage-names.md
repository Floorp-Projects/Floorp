[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [CommonMetricData](index.md) / [getStorageNames](./get-storage-names.md)

# getStorageNames

`open fun getStorageNames(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/CommonMetricData.kt#L71)

Get the list of storage names the metric will record to. This
automatically expands [DEFAULT_STORAGE_NAME](#) to the list of default
storages for the metric.

