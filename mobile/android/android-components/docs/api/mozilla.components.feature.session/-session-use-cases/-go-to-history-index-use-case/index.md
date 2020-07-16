[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [GoToHistoryIndexUseCase](./index.md)

# GoToHistoryIndexUseCase

`class GoToHistoryIndexUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L189)

Use case to jump to an arbitrary history index in a session's backstack.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates to a specific index in the [HistoryState](#) of the given session. Invalid index values will be ignored. |
