[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SettingsUseCases](../index.md) / [UpdateTrackingProtectionUseCase](./index.md)

# UpdateTrackingProtectionUseCase

`class UpdateTrackingProtectionUseCase : `[`UpdateSettingUseCase`](../-update-setting-use-case/index.md)`<`[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L66)

Updates the tracking protection policy to the given policy value when invoked.
All active sessions are automatically updated with the new policy.

### Functions

| Name | Summary |
|---|---|
| [forEachSession](for-each-session.md) | `fun forEachSession(session: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`, value: `[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called to update an active session. Defaults to updating the session's [Settings](../../../mozilla.components.concept.engine/-settings/index.md) object. |
| [update](update.md) | `fun update(settings: `[`Settings`](../../../mozilla.components.concept.engine/-settings/index.md)`, value: `[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called to update a [Settings](../../../mozilla.components.concept.engine/-settings/index.md) object using the value from the invoke call. |

### Inherited Functions

| Name | Summary |
|---|---|
| [invoke](../-update-setting-use-case/invoke.md) | `operator fun invoke(value: `[`T`](../-update-setting-use-case/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the engine setting and all active sessions. |
