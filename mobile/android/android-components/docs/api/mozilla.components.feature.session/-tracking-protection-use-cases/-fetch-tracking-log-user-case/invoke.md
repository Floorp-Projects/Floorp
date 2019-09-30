[android-components](../../../index.md) / [mozilla.components.feature.session](../../index.md) / [TrackingProtectionUseCases](../index.md) / [FetchTrackingLogUserCase](index.md) / [invoke](./invoke.md)

# invoke

`operator fun invoke(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`, onSuccess: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TrackerLog`](../../../mozilla.components.concept.engine.content.blocking/-tracker-log/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session/src/main/java/mozilla/components/feature/session/TrackingProtectionUseCases.kt#L137)

Fetch all the tracking protection logged information of a given [session](invoke.md#mozilla.components.feature.session.TrackingProtectionUseCases.FetchTrackingLogUserCase$invoke(mozilla.components.browser.session.Session, kotlin.Function1((kotlin.collections.List((mozilla.components.concept.engine.content.blocking.TrackerLog)), kotlin.Unit)), kotlin.Function1((kotlin.Throwable, kotlin.Unit)))/session).

### Parameters

`session` - the session for which loading should be stopped.

`onSuccess` - callback invoked if the data was fetched successfully.

`onError` - (optional) callback invoked if fetching the data caused an exception.