[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [registerPings](./register-pings.md)

# registerPings

`fun registerPings(pings: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L158)

Register the pings generated from `pings.yaml` with Glean.

### Parameters

`pings` - The `Pings` object generated for your library or application
by Glean.