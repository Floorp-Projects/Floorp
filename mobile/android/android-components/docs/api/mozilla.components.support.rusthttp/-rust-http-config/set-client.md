[android-components](../../index.md) / [mozilla.components.support.rusthttp](../index.md) / [RustHttpConfig](index.md) / [setClient](./set-client.md)

# setClient

`fun setClient(c: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rusthttp/src/main/java/mozilla/components/support/rusthttp/RustHttpConfig.kt#L24)

Set the HTTP client to be used by all Rust code.

The `Lazy`'s value is not read until the first request is made.

This must be called

* after initializing a megazord for users using a custom megazord build.
* before any other calls into application-services rust code which make HTTP requests.
