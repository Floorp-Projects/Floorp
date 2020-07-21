[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineSession](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GeckoEngineSession(runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)`, privateMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, geckoSessionProvider: () -> `[`GeckoSession`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html)` = {
        val settings = GeckoSessionSettings.Builder()
            .usePrivateMode(privateMode)
            .contextId(contextId)
            .build()
        GeckoSession(settings)
    }, context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO, openGeckoSession: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`

Gecko-based EngineSession implementation.

