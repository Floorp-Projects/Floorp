[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [ManifestStorage](index.md) / [warmUpScopes](./warm-up-scopes.md)

# warmUpScopes

`suspend fun warmUpScopes(currentTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ManifestStorage.kt#L94)

Populates a cache of currently installed web app scopes and their start urls.

### Parameters

`currentTime` - the current time is used to determine which web apps are still installed.