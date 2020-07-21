[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [RemoveTabUseCase](./index.md)

# RemoveTabUseCase

`interface RemoveTabUseCase` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L65)

Contract for use cases that remove a tab.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `abstract operator fun invoke(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the session with the provided ID. This method has no effect if the session doesn't exist.`abstract operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultRemoveTabUseCase](../-default-remove-tab-use-case/index.md) | `class DefaultRemoveTabUseCase : `[`RemoveTabUseCase`](./index.md)<br>Default implementation of [RemoveTabUseCase](./index.md), interacting with [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md). |
