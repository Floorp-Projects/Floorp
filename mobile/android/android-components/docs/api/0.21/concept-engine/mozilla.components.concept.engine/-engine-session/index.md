---
title: EngineSession - 
---

[mozilla.components.concept.engine](../index.html) / [EngineSession](./index.html)

# EngineSession

`abstract class EngineSession : Observable<`[`Observer`](-observer/index.html)`>`

Class representing a single engine session.

In browsers usually a session corresponds to a tab.

### Types

| [Observer](-observer/index.html) | `interface Observer`<br>Interface to be implemented by classes that want to observe this engine session. |
| [TrackingProtectionPolicy](-tracking-protection-policy/index.html) | `class TrackingProtectionPolicy`<br>Represents a tracking protection policy which is a combination of tracker categories that should be blocked. |

### Constructors

| [&lt;init&gt;](-init-.html) | `EngineSession(delegate: Observable<`[`Observer`](-observer/index.html)`> = ObserverRegistry())`<br>Class representing a single engine session. |

### Properties

| [settings](settings.html) | `abstract val settings: `[`Settings`](../-settings/index.html)<br>Provides access to the settings of this engine session. |

### Functions

| [clearFindMatches](clear-find-matches.html) | `abstract fun clearFindMatches(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the highlighted results of previous calls to [findAll](find-all.html) / [findNext](find-next.html). |
| [close](close.html) | `fun close(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Close the session. This may free underlying objects. Call this when you are finished using this session. |
| [disableTrackingProtection](disable-tracking-protection.html) | `abstract fun disableTrackingProtection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disables tracking protection for this engine session. |
| [enableTrackingProtection](enable-tracking-protection.html) | `abstract fun enableTrackingProtection(policy: `[`TrackingProtectionPolicy`](-tracking-protection-policy/index.html)` = TrackingProtectionPolicy.all()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables tracking protection for this engine session. |
| [findAll](find-all.html) | `abstract fun findAll(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights all occurrences of the provided String and highlights them asynchronously. |
| [findNext](find-next.html) | `abstract fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Finds and highlights the next or previous match found by [findAll](find-all.html). |
| [goBack](go-back.html) | `abstract fun goBack(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates back in the history of this session. |
| [goForward](go-forward.html) | `abstract fun goForward(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Navigates forward in the history of this session. |
| [loadData](load-data.html) | `abstract fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "text/html", encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "UTF-8"): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the data with the given mimeType. Example: |
| [loadUrl](load-url.html) | `abstract fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the given URL. |
| [reload](reload.html) | `abstract fun reload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Reloads the current URL. |
| [restoreState](restore-state.html) | `abstract fun restoreState(state: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Restores the engine state as provided by [saveState](save-state.html). |
| [saveState](save-state.html) | `abstract fun saveState(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`<br>Saves and returns the engine state. Engine implementations are not required to persist the state anywhere else than in the returned map. Engines that already provide a serialized state can use a single entry in this map to provide this state. The only requirement is that the same map can be used to restore the original state. See [restoreState](restore-state.html) and the specific engine implementation for details. |
| [setDesktopMode](set-desktop-mode.html) | `abstract fun setDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enables/disables Desktop Mode with an optional ability to reload the session right after. |
| [stopLoading](stop-loading.html) | `abstract fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops loading the current session. |

