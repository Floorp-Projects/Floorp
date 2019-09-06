[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineSession](./index.md)

# GeckoEngineSession

`class GeckoEngineSession : CoroutineScope, `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineSession.kt#L48)

Gecko-based EngineSession implementation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoEngineSession(runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)`, privateMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null, geckoSessionProvider: () -> `[`GeckoSession`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html)` = {
        val settings = GeckoSessionSettings.Builder()
            .usePrivateMode(privateMode)
            .build()
        GeckoSession(settings)
    }, context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO, openGeckoSession: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>`GeckoEngineSession(runtime: `[`GeckoRuntime`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html)`, privateMode: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null, geckoSessionProvider: () -> `[`GeckoSession`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html)` = {
        val settings = GeckoSessionSettings.Builder()
            .usePrivateMode(privateMode)
            .build()
        GeckoSession(settings)
    }, context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = Dispatchers.IO)`<br>Gecko-based EngineSession implementation. |

### Properties

| Name | Summary |
|---|---|
| [coroutineContext](coroutine-context.md) | `val coroutineContext: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html) |
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>See [EngineSession.settings](../../mozilla.components.concept.engine/-engine-session/settings.md) |

### Functions

| Name | Summary |
|---|---|
| [clearFindMatches](clear-find-matches.md) | `fun clearFindMatches(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.clearFindMatches](../../mozilla.components.concept.engine/-engine-session/clear-find-matches.md) |
| [close](close.md) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.close](../../mozilla.components.concept.engine/-engine-session/close.md). |
| [disableTrackingProtection](disable-tracking-protection.md) | `fun disableTrackingProtection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.disableTrackingProtection](../../mozilla.components.concept.engine/-engine-session/disable-tracking-protection.md) |
| [enableTrackingProtection](enable-tracking-protection.md) | `fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.enableTrackingProtection](../../mozilla.components.concept.engine/-engine-session/enable-tracking-protection.md) |
| [exitFullScreenMode](exit-full-screen-mode.md) | `fun exitFullScreenMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.exitFullScreenMode](../../mozilla.components.concept.engine/-engine-session/exit-full-screen-mode.md) |
| [findAll](find-all.md) | `fun findAll(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.findAll](../../mozilla.components.concept.engine/-engine-session/find-all.md) |
| [findNext](find-next.md) | `fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.findNext](../../mozilla.components.concept.engine/-engine-session/find-next.md) |
| [goBack](go-back.md) | `fun goBack(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.goBack](../../mozilla.components.concept.engine/-engine-session/go-back.md) |
| [goForward](go-forward.md) | `fun goForward(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.goForward](../../mozilla.components.concept.engine/-engine-session/go-forward.md) |
| [handleLongClick](handle-long-click.md) | `fun handleLongClick(elementSrc: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, elementType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`?` |
| [loadData](load-data.md) | `fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.loadData](../../mozilla.components.concept.engine/-engine-session/load-data.md) |
| [loadUrl](load-url.md) | `fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](../../mozilla.components.concept.engine/-engine-session/-load-url-flags/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.loadUrl](../../mozilla.components.concept.engine/-engine-session/load-url.md) |
| [recoverFromCrash](recover-from-crash.md) | `fun recoverFromCrash(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>See [EngineSession.recoverFromCrash](../../mozilla.components.concept.engine/-engine-session/recover-from-crash.md) |
| [reload](reload.md) | `fun reload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.reload](../../mozilla.components.concept.engine/-engine-session/reload.md) |
| [restoreState](restore-state.md) | `fun restoreState(state: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.restoreState](../../mozilla.components.concept.engine/-engine-session/restore-state.md) |
| [saveState](save-state.md) | `fun saveState(): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>See [EngineSession.saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md) |
| [stopLoading](stop-loading.md) | `fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.stopLoading](../../mozilla.components.concept.engine/-engine-session/stop-loading.md) |
| [toggleDesktopMode](toggle-desktop-mode.md) | `fun toggleDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.settings](../../mozilla.components.concept.engine/-engine-session/settings.md) |

### Inherited Functions

| Name | Summary |
|---|---|
| [clearData](../../mozilla.components.concept.engine/-engine-session/clear-data.md) | `open fun clearData(data: `[`BrowsingData`](../../mozilla.components.concept.engine/-engine/-browsing-data/index.md)` = Engine.BrowsingData.all(), host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears browsing data stored by the engine. |

### Extension Functions

| Name | Summary |
|---|---|
| [launchGeckoResult](../kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md) | `fun <T> CoroutineScope.launchGeckoResult(context: `[`CoroutineContext`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.coroutines/-coroutine-context/index.html)` = EmptyCoroutineContext, start: CoroutineStart = CoroutineStart.DEFAULT, block: suspend CoroutineScope.() -> `[`T`](../kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`): `[`GeckoResult`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoResult.html)`<`[`T`](../kotlinx.coroutines.-coroutine-scope/launch-gecko-result.md#T)`>`<br>Create a GeckoResult from a co-routine. |
