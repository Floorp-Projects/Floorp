[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [AddExceptionUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L40)

Adds a new tab to the exception list, as a result this tab will not get applied any
tracking protection policy.

### Parameters

`tabId` - The id of the tab that will be added to the exception list.