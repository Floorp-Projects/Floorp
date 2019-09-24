[android-components](../../../index.md) / [mozilla.components.feature.sendtab](../../index.md) / [SendTabUseCases](../index.md) / [SendToAllUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(tab: `[`TabData`](../../../mozilla.components.concept.sync/-tab-data/index.md)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sendtab/src/main/java/mozilla/components/feature/sendtab/SendTabUseCases.kt#L98)

Sends the tab to all send-tab compatible devices.

### Parameters

`tab` - The tab to send.

**Return**
a deferred boolean as true if the combined result was successful or not.

`operator fun invoke(tabs: `[`Collection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-collection/index.html)`<`[`TabData`](../../../mozilla.components.concept.sync/-tab-data/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sendtab/src/main/java/mozilla/components/feature/sendtab/SendTabUseCases.kt#L114)

Sends the tabs to all the send-tab compatible devices.

### Parameters

`tabs` - a collection of tabs to send.

**Return**
a deferred boolean as true if the combined result was successful or not.

