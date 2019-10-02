[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [ContainsExceptionUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`, onResult: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L96)

Indicates if a given [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.ContainsExceptionUseCase$invoke(mozilla.components.browser.session.Session, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) is in the exception list.

### Parameters

`session` - The [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.ContainsExceptionUseCase$invoke(mozilla.components.browser.session.Session, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) to verify.

`onResult` - A callback to inform if the given [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.ContainsExceptionUseCase$invoke(mozilla.components.browser.session.Session, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) is on
the exception list, true if it is in otherwise false.