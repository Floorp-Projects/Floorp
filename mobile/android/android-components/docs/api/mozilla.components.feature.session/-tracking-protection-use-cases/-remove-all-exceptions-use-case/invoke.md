[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [RemoveAllExceptionsUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L93)

Removes all domains from the exception list.

As we are applying this on all the sessions it could cause a
performance hit, right now we don't have a straightforward way to
avoid it, as it will required that we introduce the browser store,
we are going to address this on #6283
(https://github.com/mozilla-mobile/android-components/issues/6283).

