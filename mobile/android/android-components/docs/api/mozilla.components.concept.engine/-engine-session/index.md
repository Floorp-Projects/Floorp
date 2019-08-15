[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineSession](./index.md)

# EngineSession

`abstract class EngineSession : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineSession.kt#L29)

Class representing a single engine session.

In browsers usually a session corresponds to a tab.

### Types

| Name | Summary |
|---|---|
| [LoadUrlFlags](-load-url-flags/index.md) | `class LoadUrlFlags`<br>Describes a combination of flags provided to the engine when loading a URL. |
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe this engine session. |
| [TrackingProtectionPolicy](-tracking-protection-policy/index.md) | `open class TrackingProtectionPolicy`<br>Represents a tracking protection policy, which is a combination of tracker categories that should be blocked. Unless otherwise specified, a [TrackingProtectionPolicy](-tracking-protection-policy/index.md) is applicable to all session types (see [TrackingProtectionPolicyForSessionTypes](-tracking-protection-policy-for-session-types/index.md)). |
| [TrackingProtectionPolicyForSessionTypes](-tracking-protection-policy-for-session-types/index.md) | `class TrackingProtectionPolicyForSessionTypes : `[`TrackingProtectionPolicy`](-tracking-protection-policy/index.md)<br>Subtype of [TrackingProtectionPolicy](-tracking-protection-policy/index.md) to control the type of session this policy should be applied to. By default, a policy will be applied to all sessions. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `EngineSession(delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`<br>Class representing a single engine session. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `abstract val settings: `[`Settings`](../-settings/index.md)<br>Provides access to the settings of this engine session. |

### Functions

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `open fun clearData(data: `[`BrowsingData`](../-engine/-browsing-data/index.md)` = Engine.BrowsingData.all(), host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears browsing data stored by the engine. |
| [clearFindMatches](clear-find-matches.md) | `abstract fun clearFindMatches(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the highlighted results of previous calls to [findAll](find-all.md) / [findNext](find-next.md). |
| [close](close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Close the session. This may free underlying objects. Call this when you are finished using this session. |
| [disableTrackingProtection](disable-tracking-protection.md) | `abstract fun disableTrackingProtection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables tracking protection for this engine session. |
| [enableTrackingProtection](enable-tracking-protection.md) | `abstract fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](-tracking-protection-policy/index.md)` = TrackingProtectionPolicy.strict()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables tracking protection for this engine session. |
| [exitFullScreenMode](exit-full-screen-mode.md) | `abstract fun exitFullScreenMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Exits fullscreen mode if currently in it that state. |
| [findAll](find-all.md) | `abstract fun findAll(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights all occurrences of the provided String and highlights them asynchronously. |
| [findNext](find-next.md) | `abstract fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights the next or previous match found by [findAll](find-all.md). |
| [goBack](go-back.md) | `abstract fun goBack(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates back in the history of this session. |
| [goForward](go-forward.md) | `abstract fun goForward(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates forward in the history of this session. |
| [loadData](load-data.md) | `abstract fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "text/html", encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "UTF-8"): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the data with the given mimeType. Example: |
| [loadUrl](load-url.md) | `abstract fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, flags: `[`LoadUrlFlags`](-load-url-flags/index.md)` = LoadUrlFlags.none()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the given URL. |
| [recoverFromCrash](recover-from-crash.md) | `abstract fun recoverFromCrash(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tries to recover from a crash by restoring the last know state. |
| [reload](reload.md) | `abstract fun reload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Reloads the current URL. |
| [restoreState](restore-state.md) | `abstract fun restoreState(state: `[`EngineSessionState`](../-engine-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores the engine state as provided by [saveState](save-state.md). |
| [saveState](save-state.md) | `abstract fun saveState(): `[`EngineSessionState`](../-engine-session-state/index.md)<br>Saves and returns the engine state. Engine implementations are not required to persist the state anywhere else than in the returned map. Engines that already provide a serialized state can use a single entry in this map to provide this state. The only requirement is that the same map can be used to restore the original state. See [restoreState](restore-state.md) and the specific engine implementation for details. |
| [stopLoading](stop-loading.md) | `abstract fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops loading the current session. |
| [toggleDesktopMode](toggle-desktop-mode.md) | `abstract fun toggleDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables/disables Desktop Mode with an optional ability to reload the session right after. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoEngineSession](../../mozilla.components.browser.engine.gecko/-gecko-engine-session/index.md) | `class GeckoEngineSession : CoroutineScope, `[`EngineSession`](./index.md)<br>Gecko-based EngineSession implementation. |
| [ServoEngineSession](../../mozilla.components.browser.engine.servo/-servo-engine-session/index.md) | `class ServoEngineSession : `[`EngineSession`](./index.md)<br>Servo-based EngineSession implementation. |
| [SystemEngineSession](../../mozilla.components.browser.engine.system/-system-engine-session/index.md) | `class SystemEngineSession : `[`EngineSession`](./index.md)<br>WebView-based EngineSession implementation. |
