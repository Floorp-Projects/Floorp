[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngine](index.md) / [getTrackersLog](./get-trackers-log.md)

# getTrackersLog

`fun getTrackersLog(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngine.kt#L117)

Overrides [Engine.getTrackersLog](../../mozilla.components.concept.engine/-engine/get-trackers-log.md)

Fetch a list of trackers logged for a given [session](get-trackers-log.md#mozilla.components.browser.engine.gecko.GeckoEngine$getTrackersLog(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session) .

### Parameters

`session` - the session where the trackers were logged.

`onSuccess` - callback invoked if the data was fetched successfully.

`onError` - (optional) callback invoked if fetching the data caused an exception.