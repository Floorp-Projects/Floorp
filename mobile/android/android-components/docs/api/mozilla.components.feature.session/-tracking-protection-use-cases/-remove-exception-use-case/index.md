[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [RemoveExceptionUseCase](./index.md)

# RemoveExceptionUseCase

`class RemoveExceptionUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L51)

Use case for removing a tab or a [TrackingProtectionException](../../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md) from the exception list.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a tab from the exception list.`operator fun invoke(exception: `[`TrackingProtectionException`](../../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes a [exception](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.RemoveExceptionUseCase$invoke(mozilla.components.concept.engine.content.blocking.TrackingProtectionException)/exception) from the exception list. |
