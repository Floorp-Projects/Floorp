[android-components](../../index.md) / [mozilla.components.feature.addons.amo](../index.md) / [AddonCollectionProvider](./index.md)

# AddonCollectionProvider

`class AddonCollectionProvider : `[`AddonsProvider`](../../mozilla.components.feature.addons/-addons-provider/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/amo/AddonCollectionProvider.kt#L49)

Provide access to the collections AMO API.
https://addons-server.readthedocs.io/en/latest/topics/api/collections.html

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddonCollectionProvider(context: <ERROR CLASS>, client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, serverURL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_SERVER_URL, collectionName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = DEFAULT_COLLECTION_NAME, maxCacheAgeInMinutes: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = -1)`<br>Provide access to the collections AMO API. https://addons-server.readthedocs.io/en/latest/topics/api/collections.html |

### Functions

| Name | Summary |
|---|---|
| [getAddonIconBitmap](get-addon-icon-bitmap.md) | `suspend fun getAddonIconBitmap(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`): <ERROR CLASS>?`<br>Fetches given Addon icon from the url and returns a decoded Bitmap |
| [getAvailableAddons](get-available-addons.md) | `suspend fun getAvailableAddons(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, readTimeoutInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>`<br>Interacts with the collections endpoint to provide a list of available add-ons. May return a cached response, if available, not expired (see [maxCacheAgeInMinutes](#)) and allowed (see [allowCache](get-available-addons.md#mozilla.components.feature.addons.amo.AddonCollectionProvider$getAvailableAddons(kotlin.Boolean, kotlin.Long)/allowCache)). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
