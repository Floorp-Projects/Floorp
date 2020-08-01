[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks](../index.md) / [AndroidAssetFinder](index.md) / [getAndroidAppAsset](./get-android-app-asset.md)

# getAndroidAppAsset

`fun getAndroidAppAsset(packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, packageManager: <ERROR CLASS>): `[`Sequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.sequences/-sequence/index.html)`<`[`Android`](../-asset-descriptor/-android/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/AndroidAssetFinder.kt#L33)

Converts the Android App with the given package name into an asset descriptor
by computing the SHA256 certificate for each signing signature.

The output is lazily computed. If desired, only the first item from the sequence could
be used and other certificates (if any) will not be computed.

