[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SettingsUseCases](../index.md) / [UpdateSettingUseCase](./index.md)

# UpdateSettingUseCase

`abstract class UpdateSettingUseCase<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L30)

Use case to update a setting and then change all
active browsing sessions to use the new setting.

### Parameters

`engineSettings` - the engine's [Settings](../../../mozilla.components.concept.engine/-settings/index.md). The first settings object that is updated.

`sessionManager` - the application's [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md). Used to query the active sessions.

### Functions

| Name | Summary |
|---|---|
| [forEachSession](for-each-session.md) | `open fun forEachSession(session: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`, value: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called to update an active session. Defaults to updating the session's [Settings](../../../mozilla.components.concept.engine/-settings/index.md) object. |
| [invoke](invoke.md) | `operator fun invoke(value: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the engine setting and all active sessions. |
| [update](update.md) | `abstract fun update(settings: `[`Settings`](../../../mozilla.components.concept.engine/-settings/index.md)`, value: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called to update a [Settings](../../../mozilla.components.concept.engine/-settings/index.md) object using the value from the invoke call. |

### Inheritors

| Name | Summary |
|---|---|
| [UpdateTrackingProtectionUseCase](../-update-tracking-protection-use-case/index.md) | `class UpdateTrackingProtectionUseCase : `[`UpdateSettingUseCase`](./index.md)`<`[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`>`<br>Updates the tracking protection policy to the given policy value when invoked. All active sessions are automatically updated with the new policy. |
