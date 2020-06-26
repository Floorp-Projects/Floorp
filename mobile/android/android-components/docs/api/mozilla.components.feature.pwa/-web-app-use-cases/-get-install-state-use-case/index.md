[android-components](../../../index.md) / [mozilla.components.feature.pwa](../../index.md) / [WebAppUseCases](../index.md) / [GetInstallStateUseCase](./index.md)

# GetInstallStateUseCase

`class GetInstallStateUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppUseCases.kt#L88)

Checks the current install state of a Web App.

Returns WebAppShortcutManager.InstallState.Installed if the user has installed
or used the web app in the past 30 days.

Otherwise, WebAppShortcutManager.InstallState.NotInstalled is returned.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator suspend fun invoke(currentTimeMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`WebAppInstallState`](../../-web-app-shortcut-manager/-web-app-install-state/index.md)`?` |
