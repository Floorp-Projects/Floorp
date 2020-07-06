[android-components](../../../index.md) / [mozilla.components.feature.tabs](../../index.md) / [TabsUseCases](../index.md) / [AddNewPrivateTabUseCase](./index.md)

# AddNewPrivateTabUseCase

`class AddNewPrivateTabUseCase : `[`LoadUrlUseCase`](../../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L166)

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)`, additionalHeaders: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`operator fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selectTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, startLoading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, parentId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, flags: `[`LoadUrlFlags`](../../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)` = LoadUrlFlags.none(), engineSession: `[`EngineSession`](../../../mozilla.components.concept.engine/-engine-session/index.md)`? = null): `[`Session`](../../../mozilla.components.browser.session/-session/index.md)<br>Adds a new private tab and loads the provided URL. |
