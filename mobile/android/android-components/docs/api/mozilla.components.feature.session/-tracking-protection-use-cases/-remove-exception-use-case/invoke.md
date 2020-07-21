[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [RemoveExceptionUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L61)

Removes a tab from the exception list.

### Parameters

`tabId` - The id of the tab that will be removed from the exception list.`operator fun invoke(exception: `[`TrackingProtectionException`](../../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L72)

Removes a [exception](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.RemoveExceptionUseCase$invoke(mozilla.components.concept.engine.content.blocking.TrackingProtectionException)/exception) from the exception list.

### Parameters

`exception` - The [TrackingProtectionException](../../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md) that will be removed from the exception list.