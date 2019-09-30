[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [FetchExceptionsUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(onResult: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L118)

Fetch all domains that will be ignored for tracking protection.

### Parameters

`onResult` - A callback to inform that the domains on the exception list has been fetched,
it provides a list of domains that are on the exception list, if there are none domains
on the exception list, an empty list will be provided.