[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [StopLoadingUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L144)

Stops the current URL of the provided session from loading.

### Parameters

`session` - the session for which loading should be stopped.