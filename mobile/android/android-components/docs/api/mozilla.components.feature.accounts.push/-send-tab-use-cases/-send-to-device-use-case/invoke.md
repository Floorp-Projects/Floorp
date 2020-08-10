[android-components](../../../index.md) / [mozilla.components.feature.accounts.push](../../index.md) / [SendTabUseCases](../index.md) / [SendToDeviceUseCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(deviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tab: `[`TabData`](../../../mozilla.components.concept.sync/-tab-data/index.md)`): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts-push/src/main/java/mozilla/components/feature/accounts/push/SendTabUseCases.kt#L52)

Sends the tab to provided deviceId if possible.

### Parameters

`deviceId` - The deviceId to send the tab.

`tab` - The tab to send.

**Return**
a deferred boolean if the result was successful or not.

`operator fun invoke(deviceId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TabData`](../../../mozilla.components.concept.sync/-tab-data/index.md)`>): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts-push/src/main/java/mozilla/components/feature/accounts/push/SendTabUseCases.kt#L62)

Sends the tabs to provided deviceId if possible.

### Parameters

`deviceId` - The deviceId to send the tab.

`tabs` - The list of tabs to send.

**Return**
a deferred boolean as true if the combined result was successful or not.

