[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [GoToHistoryIndexUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(index: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = sessionManager.selectedSession): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L199)

Navigates to a specific index in the [HistoryState](#) of the given session.
Invalid index values will be ignored.

### Parameters

`index` - the index in the session's [HistoryState](#) to navigate to

`session` - the session whose [HistoryState](#) is being accessed