[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SettingsUseCases](../index.md) / [UpdateTrackingProtectionUseCase](index.md) / [forEachSession](./for-each-session.md)

# forEachSession

`protected fun forEachSession(session: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`, value: `[`TrackingProtectionPolicy`](../../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SettingsUseCases.kt#L75)

Overrides [UpdateSettingUseCase.forEachSession](../-update-setting-use-case/for-each-session.md)

Called to update an active session. Defaults to updating the session's [Settings](../../../mozilla.components.concept.engine/-settings/index.md) object.

