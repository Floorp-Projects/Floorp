[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddOnsProvider](./index.md)

# AddOnsProvider

`interface AddOnsProvider` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddOnsProvider.kt#L10)

A contract that indicate how an add-on provider must behave.

### Functions

| Name | Summary |
|---|---|
| [getAvailableAddOns](get-available-add-ons.md) | `abstract suspend fun getAvailableAddOns(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AddOn`](../-add-on/index.md)`>`<br>Provides a list of all available add-ons. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [AddOnsCollectionsProvider](../../mozilla.components.feature.addons.amo/-add-ons-collections-provider/index.md) | `class AddOnsCollectionsProvider : `[`AddOnsProvider`](./index.md)<br>Provide access to the collections AMO API. https://addons-server.readthedocs.io/en/latest/topics/api/collections.html |
