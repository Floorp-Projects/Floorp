[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [DefaultSelectTabUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L47)

Overrides [SelectTabUseCase.invoke](../-select-tab-use-case/invoke.md)

Marks the provided session as selected.

### Parameters

`session` - The session to select.`fun invoke(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L54)

Overrides [SelectTabUseCase.invoke](../-select-tab-use-case/invoke.md)

Marks the tab with the provided [tabId](invoke.md#mozilla.components.feature.tabs.TabsUseCases.DefaultSelectTabUseCase$invoke(kotlin.String)/tabId) as selected.

