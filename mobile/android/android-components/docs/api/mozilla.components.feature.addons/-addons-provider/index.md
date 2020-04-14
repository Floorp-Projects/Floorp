[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonsProvider](./index.md)

# AddonsProvider

`interface AddonsProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonsProvider.kt#L10)

A contract that indicate how an add-on provider must behave.

### Functions

| Name | Summary |
|---|---|
| [getAvailableAddons](get-available-addons.md) | `abstract suspend fun getAvailableAddons(allowCache: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, readTimeoutInSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../-addon/index.md)`>`<br>Provides a list of all available add-ons. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddonCollectionProvider](../../mozilla.components.feature.addons.amo/-addon-collection-provider/index.md) | `class AddonCollectionProvider : `[`AddonsProvider`](./index.md)<br>Provide access to the collections AMO API. https://addons-server.readthedocs.io/en/latest/topics/api/collections.html |
