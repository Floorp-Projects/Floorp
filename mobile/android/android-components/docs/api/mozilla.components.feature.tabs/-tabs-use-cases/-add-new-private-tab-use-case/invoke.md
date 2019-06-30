[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [AddNewPrivateTabUseCase](index.md) / [invoke](./invoke.md)

# invoke

`fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L91)

Overrides [LoadUrlUseCase.invoke](../../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/invoke.md)

Adds a new private tab and loads the provided URL.

### Parameters

`url` - The URL to be loaded in the new private tab.`operator fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, startLoading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, parent: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`? = null): `[`Session`](../../../mozilla.components.browser.session/-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L103)

Adds a new private tab and loads the provided URL.

### Parameters

`url` - The URL to be loaded in the new tab.

`selectTab` - True (default) if the new tab should be selected immediately.

`startLoading` - True (default) if the new tab should start loading immediately.

`parent` - the parent session to use for the newly created session.