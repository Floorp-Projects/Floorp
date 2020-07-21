[android-components](../index.md) / [mozilla.components.service.fxa.manager.ext](index.md) / [withConstellation](./with-constellation.md)

# withConstellation

`inline fun `[`FxaAccountManager`](../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`.withConstellation(block: `[`DeviceConstellation`](../mozilla.components.concept.sync/-device-constellation/index.md)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/ext/FxaAccountManager.kt#L14)

Executes [block](with-constellation.md#mozilla.components.service.fxa.manager.ext$withConstellation(mozilla.components.service.fxa.manager.FxaAccountManager, kotlin.Function1((mozilla.components.concept.sync.DeviceConstellation, kotlin.Unit)))/block) and provides the [DeviceConstellation](../mozilla.components.concept.sync/-device-constellation/index.md) of an [OAuthAccount](../mozilla.components.concept.sync/-o-auth-account/index.md) if present.

