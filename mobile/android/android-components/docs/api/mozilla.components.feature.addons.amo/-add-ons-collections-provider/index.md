[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddOnsCollectionsProvider](./index.md)

# AddOnsCollectionsProvider

`class AddOnsCollectionsProvider : `[`AddOnsProvider`](../../mozilla.components.feature.addons/-add-ons-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddOnsCollectionsProvider.kt#L32)

Provide access to the collections AMO API.
https://addons-server.readthedocs.io/en/latest/topics/api/collections.html

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddOnsCollectionsProvider(serverURL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_SERVER_URL, collectionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_COLLECTION_NAME, client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`<br>Provide access to the collections AMO API. https://addons-server.readthedocs.io/en/latest/topics/api/collections.html |

### Functions

| Name | Summary |
|---|---|
| [getAvailableAddOns](get-available-add-ons.md) | `suspend fun getAvailableAddOns(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AddOn`](../../mozilla.components.feature.addons/-add-on/index.md)`>`<br>Interacts with the collections endpoint to provide a list of available add-ons. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
