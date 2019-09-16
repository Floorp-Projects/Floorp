[android-components](../../index.md) / [mozilla.components.feature.pwa](../index.md) / [WebAppUseCases](./index.md)

# WebAppUseCases

`class WebAppUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/WebAppUseCases.kt#L16)

These use cases allow for adding a web app or web site to the homescreen.

### Types

| Name | Summary |
|---|---|
| [AddToHomescreenUseCase](-add-to-homescreen-use-case/index.md) | `class AddToHomescreenUseCase`<br>Let the user add the selected session to the homescreen. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppUseCases(applicationContext: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, supportWebApps: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>These use cases allow for adding a web app or web site to the homescreen. |

### Properties

| Name | Summary |
|---|---|
| [addToHomescreen](add-to-homescreen.md) | `val addToHomescreen: `[`AddToHomescreenUseCase`](-add-to-homescreen-use-case/index.md) |

### Functions

| Name | Summary |
|---|---|
| [isInstallable](is-installable.md) | `fun isInstallable(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks to see if the current session can be installed as a Progressive Web App. |
| [isPinningSupported](is-pinning-supported.md) | `fun isPinningSupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the launcher supports adding shortcuts. |
