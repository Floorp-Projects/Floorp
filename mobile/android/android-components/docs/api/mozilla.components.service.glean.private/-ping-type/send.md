[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [PingType](index.md) / [send](./send.md)

# send

`fun send(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/PingType.kt#L42)

Send the ping.

While the collection of metrics into pings happens synchronously, the
ping queuing and ping uploading happens asyncronously.
There are no guarantees that this will happen immediately.

If the ping currently contains no content, it will not be queued.

