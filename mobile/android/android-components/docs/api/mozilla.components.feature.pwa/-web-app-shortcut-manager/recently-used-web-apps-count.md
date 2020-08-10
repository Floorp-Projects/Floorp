[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [recentlyUsedWebAppsCount](./recently-used-web-apps-count.md)

# recentlyUsedWebAppsCount

`suspend fun recentlyUsedWebAppsCount(activeThresholdMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = ManifestStorage.ACTIVE_THRESHOLD_MS): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L224)

Counts number of recently used web apps. See [ManifestStorage.activeThresholdMs](#).

### Parameters

`activeThresholdMs` - defines a time window within which a web app is considered recently used.
Defaults to [ManifestStorage.ACTIVE_THRESHOLD_MS](../-manifest-storage/-a-c-t-i-v-e_-t-h-r-e-s-h-o-l-d_-m-s.md).

**Return**
count of recently used web apps

