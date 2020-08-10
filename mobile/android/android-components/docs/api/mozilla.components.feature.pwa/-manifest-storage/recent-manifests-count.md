[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [recentManifestsCount](./recent-manifests-count.md)

# recentManifestsCount

`suspend fun recentManifestsCount(activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = this.activeThresholdMs, currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L68)

Counts number of recently used manifests, as configured by [activeThresholdMs](recent-manifests-count.md#mozilla.components.feature.pwa.ManifestStorage$recentManifestsCount(kotlin.Long, kotlin.Long)/activeThresholdMs).

### Parameters

`currentTimeMs` - current time, exposed for testing

`activeThresholdMs` - a time threshold within which manifests are considered to be recently used.