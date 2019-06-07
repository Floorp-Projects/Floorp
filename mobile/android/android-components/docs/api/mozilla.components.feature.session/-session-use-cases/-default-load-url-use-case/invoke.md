[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [DefaultLoadUrlUseCase](index.md) / [invoke](./invoke.md)

# invoke

`fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L45)

Overrides [LoadUrlUseCase.invoke](../-load-url-use-case/invoke.md)

Loads the provided URL using the currently selected session. If
there's no selected session a new session will be created using
[onNoSession](#).

### Parameters

`url` - The URL to be loaded using the selected session.`fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L57)

Loads the provided URL using the specified session. If no session
is provided the currently selected session will be used. If there's
no selected session a new session will be created using [onNoSession](#).

### Parameters

`url` - The URL to be loaded using the provided session.

`session` - the session for which the URL should be loaded.