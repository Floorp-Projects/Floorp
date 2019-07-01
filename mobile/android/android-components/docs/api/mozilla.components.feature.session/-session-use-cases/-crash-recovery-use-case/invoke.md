[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [CrashRecoveryUseCase](index.md) / [invoke](./invoke.md)

# invoke

`fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L200)

Tries to recover the state of the provided [Session](../../../mozilla.components.browser.session/-session/index.md).

`fun invoke(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L205)

Tries to recover the state of all crashed [Session](../../../mozilla.components.browser.session/-session/index.md)s (with [Session.crashed](../../../mozilla.components.browser.session/-session/crashed.md) flag set).

`fun invoke(sessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Session`](../../../mozilla.components.browser.session/-session/index.md)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L212)

Tries to recover the state of all [sessions](invoke.md#mozilla.components.feature.session.SessionUseCases.CrashRecoveryUseCase$invoke(kotlin.collections.List((mozilla.components.browser.session.Session)))/sessions).

