[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [AndroidAssetFinder](./index.md)

# AndroidAssetFinder

`class AndroidAssetFinder` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/AndroidAssetFinder.kt#L24)

Get the SHA256 certificate for an installed Android app.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AndroidAssetFinder()`<br>Get the SHA256 certificate for an installed Android app. |

### Functions

| Name | Summary |
|---|---|
| [getAndroidAppAsset](get-android-app-asset.md) | `fun getAndroidAppAsset(packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, packageManager: <ERROR CLASS>): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Android`](../-asset-descriptor/-android/index.md)`>`<br>Converts the Android App with the given package name into an asset descriptor by computing the SHA256 certificate for each signing signature. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
