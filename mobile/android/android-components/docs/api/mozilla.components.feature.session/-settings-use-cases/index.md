[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SettingsUseCases](./index.md)

# SettingsUseCases

`class SettingsUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L20)

Contains use cases related to engine [Settings](../../mozilla.components.concept.engine/-settings/index.md).

### Parameters

`engine` - reference to the application's browser [Engine](../../mozilla.components.concept.engine/-engine/index.md).

`store` - the application's [BrowserStore](../../mozilla.components.browser.state.store/-browser-store/index.md).

### Types

| Name | Summary |
|---|---|
| [UpdateTrackingProtectionUseCase](-update-tracking-protection-use-case/index.md) | `class UpdateTrackingProtectionUseCase`<br>Updates the tracking protection policy to the given policy value when invoked. All active sessions are automatically updated with the new policy. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SettingsUseCases(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`)`<br>Contains use cases related to engine [Settings](../../mozilla.components.concept.engine/-settings/index.md). |

### Properties

| Name | Summary |
|---|---|
| [updateTrackingProtection](update-tracking-protection.md) | `val updateTrackingProtection: `[`UpdateTrackingProtectionUseCase`](-update-tracking-protection-use-case/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
