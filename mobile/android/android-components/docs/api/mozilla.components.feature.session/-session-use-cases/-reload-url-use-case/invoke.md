[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [ReloadUrlUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L114)

Reloads the current URL of the provided session (or the currently
selected session if none is provided).

### Parameters

`session` - the session for which reload should be triggered.

`flags` - the [LoadUrlFlags](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) to use when reloading the given session.`operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L129)

Reloads the current page of the tab with the given [tabId](invoke.md#mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase$invoke(kotlin.String, mozilla.components.concept.engine.EngineSession.LoadUrlFlags)/tabId).

### Parameters

`tabId` - id of the tab that should be reloaded

`flags` - the [LoadUrlFlags](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) to use when reloading the given tabId.