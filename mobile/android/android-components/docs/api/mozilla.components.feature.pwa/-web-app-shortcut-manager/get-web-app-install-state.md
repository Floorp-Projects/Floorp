[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppShortcutManager](index.md) / [getWebAppInstallState](./get-web-app-install-state.md)

# getWebAppInstallState

`suspend fun getWebAppInstallState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`WebAppInstallState`](-web-app-install-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppShortcutManager.kt#L206)

Checks if there is a currently installed web app to which this URL belongs.

### Parameters

`url` - url that is covered by the scope of a web app installed by the user

`currentTimeMs` - the current time in milliseconds, exposed for testing