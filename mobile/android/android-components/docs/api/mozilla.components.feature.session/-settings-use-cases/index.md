[android-components](../../index.md) / [mozilla.components.feature.session](../index.md) / [SettingsUseCases](./index.md)

# SettingsUseCases

`class SettingsUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L18)

Contains use cases related to user settings.

### Parameters

`engineSettings` - the engine's [Settings](../../mozilla.components.concept.engine/-settings/index.md).

`sessionManager` - the application's [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md).

### Types

| Name | Summary |
|---|---|
| [UpdateSettingUseCase](-update-setting-use-case/index.md) | `abstract class UpdateSettingUseCase<T>`<br>Use case to update a setting and then change all active browsing sessions to use the new setting. |
| [UpdateTrackingProtectionUseCase](-update-tracking-protection-use-case/index.md) | `class UpdateTrackingProtectionUseCase : `[`UpdateSettingUseCase`](-update-setting-use-case/index.md)`<`[`TrackingProtectionPolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`>`<br>Updates the tracking protection policy to the given policy value when invoked. All active sessions are automatically updated with the new policy. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SettingsUseCases(engineSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>Contains use cases related to user settings. |

### Properties

| Name | Summary |
|---|---|
| [updateTrackingProtection](update-tracking-protection.md) | `val updateTrackingProtection: `[`UpdateTrackingProtectionUseCase`](-update-tracking-protection-use-case/index.md) |
