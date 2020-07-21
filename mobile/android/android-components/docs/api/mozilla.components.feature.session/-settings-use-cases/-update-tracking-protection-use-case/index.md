[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SettingsUseCases](../index.md) / [UpdateTrackingProtectionUseCase](./index.md)

# UpdateTrackingProtectionUseCase

`class UpdateTrackingProtectionUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L28)

Updates the tracking protection policy to the given policy value when invoked.
All active sessions are automatically updated with the new policy.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(policy: `[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the tracking protection policy for all current and future [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) instances. |
