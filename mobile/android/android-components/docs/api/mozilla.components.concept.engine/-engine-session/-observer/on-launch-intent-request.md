[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineSession](../index.md) / [Observer](index.md) / [onLaunchIntentRequest](./on-launch-intent-request.md)

# onLaunchIntentRequest

`open fun onLaunchIntentRequest(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, appIntent: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L112)

The engine received a request to launch a app intent.

### Parameters

`url` - The string url that was requested.

`appIntent` - The Android Intent that was requested.
web content (as opposed to via the browser chrome).