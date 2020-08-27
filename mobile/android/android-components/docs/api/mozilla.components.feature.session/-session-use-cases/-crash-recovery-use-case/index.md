[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [SessionUseCases](../index.md) / [CrashRecoveryUseCase](./index.md)

# CrashRecoveryUseCase

`class CrashRecoveryUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/SessionUseCases.kt#L323)

Tries to recover from a crash by restoring the last know state.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to recover the state of all crashed sessions.`fun invoke(tabIds: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to recover the state of all sessions. |
