[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [DefaultRemoveTabUseCase](./index.md)

# DefaultRemoveTabUseCase

`class DefaultRemoveTabUseCase : `[`RemoveTabUseCase`](../-remove-tab-use-case/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L86)

Default implementation of [RemoveTabUseCase](../-remove-tab-use-case/index.md), interacting
with [SessionManager](../../../mozilla.components.browser.session/-session-manager/index.md).

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the session with the provided ID. This method has no effect if the session doesn't exist.`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the provided session. |
