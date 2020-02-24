[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [DefaultSelectTabUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L44)

Overrides [SelectTabUseCase.invoke](../-select-tab-use-case/invoke.md)

Marks the provided session as selected.

### Parameters

`session` - The session to select.`fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L48)

Overrides [SelectTabUseCase.invoke](../-select-tab-use-case/invoke.md)

Select [Session](../../../mozilla.components.browser.session/-session/index.md) with the given [tabId](../-select-tab-use-case/invoke.md#mozilla.components.feature.tabs.TabsUseCases.SelectTabUseCase$invoke(kotlin.String)/tabId).

