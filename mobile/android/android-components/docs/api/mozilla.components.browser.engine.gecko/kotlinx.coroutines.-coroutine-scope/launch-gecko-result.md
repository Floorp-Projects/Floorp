[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [kotlinx.coroutines.CoroutineScope](index.md) / [launchGeckoResult](./launch-gecko-result.md)

# launchGeckoResult

`fun <T> CoroutineScope.launchGeckoResult(context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = EmptyCoroutineContext, start: CoroutineStart = CoroutineStart.DEFAULT, block: suspend CoroutineScope.() -> `[`T`](launch-gecko-result.md#T)`): `[`GeckoResult`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html)`<`[`T`](launch-gecko-result.md#T)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoResult.kt#L34)

Create a GeckoResult from a co-routine.

