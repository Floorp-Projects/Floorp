[android-components](../../index.md) / [mozilla.components.browser.engine.servo](../index.md) / [ServoEngineSession](./index.md)

# ServoEngineSession

`class ServoEngineSession : `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-servo/src/main/java/mozilla/components/browser/engine/servo/ServoEngineSession.kt#L20)

Servo-based EngineSession implementation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ServoEngineSession(defaultSettings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)`? = null)`<br>Servo-based EngineSession implementation. |

### Properties

| Name | Summary |
|---|---|
| [settings](settings.md) | `val settings: `[`Settings`](../../mozilla.components.concept.engine/-settings/index.md)<br>Provides access to the settings of this engine session. |

### Functions

| Name | Summary |
|---|---|
| [clearData](clear-data.md) | `fun clearData(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears all user data sources available. |
| [clearFindMatches](clear-find-matches.md) | `fun clearFindMatches(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the highlighted results of previous calls to [findAll](../../mozilla.components.concept.engine/-engine-session/find-all.md) / [findNext](../../mozilla.components.concept.engine/-engine-session/find-next.md). |
| [disableTrackingProtection](disable-tracking-protection.md) | `fun disableTrackingProtection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables tracking protection for this engine session. |
| [enableTrackingProtection](enable-tracking-protection.md) | `fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](../../mozilla.components.concept.engine/-engine-session/-tracking-protection-policy/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables tracking protection for this engine session. |
| [exitFullScreenMode](exit-full-screen-mode.md) | `fun exitFullScreenMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Exits fullscreen mode if currently in it that state. |
| [findAll](find-all.md) | `fun findAll(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights all occurrences of the provided String and highlights them asynchronously. |
| [findNext](find-next.md) | `fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights the next or previous match found by [findAll](../../mozilla.components.concept.engine/-engine-session/find-all.md). |
| [goBack](go-back.md) | `fun goBack(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates back in the history of this session. |
| [goForward](go-forward.md) | `fun goForward(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates forward in the history of this session. |
| [loadData](load-data.md) | `fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the data with the given mimeType. Example: |
| [loadUrl](load-url.md) | `fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the given URL. |
| [reload](reload.md) | `fun reload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Reloads the current URL. |
| [restoreState](restore-state.md) | `fun restoreState(state: `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores the engine state as provided by [saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md). |
| [saveState](save-state.md) | `fun saveState(): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md)<br>Saves and returns the engine state. Engine implementations are not required to persist the state anywhere else than in the returned map. Engines that already provide a serialized state can use a single entry in this map to provide this state. The only requirement is that the same map can be used to restore the original state. See [restoreState](../../mozilla.components.concept.engine/-engine-session/restore-state.md) and the specific engine implementation for details. |
| [stopLoading](stop-loading.md) | `fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops loading the current session. |
| [toggleDesktopMode](toggle-desktop-mode.md) | `fun toggleDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables/disables Desktop Mode with an optional ability to reload the session right after. |

### Inherited Functions

| Name | Summary |
|---|---|
| [close](../../mozilla.components.concept.engine/-engine-session/close.md) | `open fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Close the session. This may free underlying objects. Call this when you are finished using this session. |
