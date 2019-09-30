[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [AddExceptionUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L39)

Adds a new [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.AddExceptionUseCase$invoke(mozilla.components.browser.session.Session)/session) to the exception list, as a result this session will
not get applied any tracking protection policy.

### Parameters

`session` - The [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.AddExceptionUseCase$invoke(mozilla.components.browser.session.Session)/session) that will be added to the exception list.