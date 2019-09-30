[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [AddExceptionUseCase](./index.md)

# AddExceptionUseCase

`class AddExceptionUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L28)

Use case for adding a new [Session](../../../mozilla.components.browser.session/-session/index.md) to the exception list.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.AddExceptionUseCase$invoke(mozilla.components.browser.session.Session)/session) to the exception list, as a result this session will not get applied any tracking protection policy. |
