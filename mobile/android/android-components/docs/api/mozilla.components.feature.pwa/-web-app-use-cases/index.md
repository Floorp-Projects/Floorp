[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppUseCases](./index.md)

# WebAppUseCases

`class WebAppUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppUseCases.kt#L16)

These use cases allow for adding a web app or web site to the homescreen.

### Types

| Name | Summary |
|---|---|
| [AddToHomescreenUseCase](-add-to-homescreen-use-case/index.md) | `class AddToHomescreenUseCase`<br>Let the user add the selected session to the homescreen. |
| [GetInstallStateUseCase](-get-install-state-use-case/index.md) | `class GetInstallStateUseCase`<br>Checks the current install state of a Web App. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppUseCases(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, supportWebApps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)``WebAppUseCases(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, shortcutManager: `[`WebAppShortcutManager`](../-web-app-shortcut-manager/index.md)`)`<br>These use cases allow for adding a web app or web site to the homescreen. |

### Properties

| Name | Summary |
|---|---|
| [addToHomescreen](add-to-homescreen.md) | `val addToHomescreen: `[`AddToHomescreenUseCase`](-add-to-homescreen-use-case/index.md) |
| [getInstallState](get-install-state.md) | `val getInstallState: `[`GetInstallStateUseCase`](-get-install-state-use-case/index.md) |

### Functions

| Name | Summary |
|---|---|
| [isInstallable](is-installable.md) | `fun isInstallable(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks to see if the current session can be installed as a Progressive Web App. |
| [isPinningSupported](is-pinning-supported.md) | `fun isPinningSupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the launcher supports adding shortcuts. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
