[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [AssetDescriptor](./index.md)

# AssetDescriptor

`sealed class AssetDescriptor` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/AssetDescriptor.kt#L13)

Uniquely identifies an asset.

A digital asset is an identifiable and addressable online entity that typically provides some
service or content.

### Types

| Name | Summary |
|---|---|
| [Android](-android/index.md) | `data class Android : `[`AssetDescriptor`](./index.md)<br>An Android app asset descriptor. |
| [Web](-web/index.md) | `data class Web : `[`AssetDescriptor`](./index.md)<br>A web site asset descriptor. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Android](-android/index.md) | `data class Android : `[`AssetDescriptor`](./index.md)<br>An Android app asset descriptor. |
| [Web](-web/index.md) | `data class Web : `[`AssetDescriptor`](./index.md)<br>A web site asset descriptor. |
