[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [sendPings](./send-pings.md)

# sendPings

`fun sendPings(pingNames: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L358)

Send a list of pings by name.

Only custom pings (pings not managed by Glean itself) will be sent by
this function. If the name of a Glean-managed ping is passed in, an
error is logged to logcat.

While the collection of metrics into pings happens synchronously, the
ping queuing and ping uploading happens asyncronously.
There are no guarantees that this will happen immediately.

If the ping currently contains no content, it will not be sent.

### Parameters

`pingNames` - List of pings to send.

**Return**
true if any pings had content and were queued for uploading

