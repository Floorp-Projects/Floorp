[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [DefaultRemoveTabUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L96)

Overrides [RemoveTabUseCase.invoke](../-remove-tab-use-case/invoke.md)

Removes the session with the provided ID. This method
has no effect if the session doesn't exist.

### Parameters

`sessionId` - The ID of the session to remove.`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L108)

Overrides [RemoveTabUseCase.invoke](../-remove-tab-use-case/invoke.md)

Removes the provided session.

### Parameters

`session` - The session to remove.