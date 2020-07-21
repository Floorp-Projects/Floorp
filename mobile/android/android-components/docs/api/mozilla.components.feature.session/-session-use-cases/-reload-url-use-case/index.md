[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [ReloadUrlUseCase](./index.md)

# ReloadUrlUseCase

`class ReloadUrlUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L104)

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Reloads the current URL of the provided session (or the currently selected session if none is provided).`operator fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Reloads the current page of the tab with the given [tabId](invoke.md#mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase$invoke(kotlin.String, mozilla.components.concept.engine.EngineSession.LoadUrlFlags)/tabId). |
