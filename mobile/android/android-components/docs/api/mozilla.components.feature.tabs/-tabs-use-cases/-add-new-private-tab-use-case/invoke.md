[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [AddNewPrivateTabUseCase](index.md) / [invoke](./invoke.md)

# invoke

`fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L104)

Overrides [LoadUrlUseCase.invoke](../../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/invoke.md)

Adds a new private tab and loads the provided URL.

### Parameters

`url` - The URL to be loaded in the new private tab.

`flags` - the [LoadUrlFlags](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) to use when loading the provided URL.`operator fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, startLoading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Session`](../../../mozilla.components.browser.session/-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L117)

Adds a new private tab and loads the provided URL.

### Parameters

`url` - The URL to be loaded in the new tab.

`selectTab` - True (default) if the new tab should be selected immediately.

`startLoading` - True (default) if the new tab should start loading immediately.

`parentId` - the id of the parent tab to use for the newly created tab.

`flags` - the [LoadUrlFlags](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md) to use when loading the provided URL.