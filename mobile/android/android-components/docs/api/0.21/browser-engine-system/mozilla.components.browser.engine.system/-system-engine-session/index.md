---
title: SystemEngineSession - 
---

[mozilla.components.browser.engine.system](../index.html) / [SystemEngineSession](./index.html)

# SystemEngineSession

`class SystemEngineSession : EngineSession`

WebView-based EngineSession implementation.

### Constructors

| [&lt;init&gt;](-init-.html) | `SystemEngineSession(defaultSettings: Settings? = null)`<br>WebView-based EngineSession implementation. |

### Properties

| [settings](settings.html) | `val settings: Settings`<br>See [EngineSession.settings](#) |

### Functions

| [clearFindMatches](clear-find-matches.html) | `fun clearFindMatches(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.clearFindResults](#) |
| [disableTrackingProtection](disable-tracking-protection.html) | `fun disableTrackingProtection(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.disableTrackingProtection](#) |
| [enableTrackingProtection](enable-tracking-protection.html) | `fun enableTrackingProtection(policy: TrackingProtectionPolicy): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.enableTrackingProtection](#) |
| [findAll](find-all.html) | `fun findAll(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.findAll](#) |
| [findNext](find-next.html) | `fun findNext(forward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.findNext](#) |
| [goBack](go-back.html) | `fun goBack(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.goBack](#) |
| [goForward](go-forward.html) | `fun goForward(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.goForward](#) |
| [loadData](load-data.html) | `fun loadData(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.loadData](#) |
| [loadUrl](load-url.html) | `fun loadUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.loadUrl](#) |
| [reload](reload.html) | `fun reload(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.reload](#) |
| [restoreState](restore-state.html) | `fun restoreState(state: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.restoreState](#) |
| [saveState](save-state.html) | `fun saveState(): `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`>`<br>See [EngineSession.saveState](#) |
| [setDesktopMode](set-desktop-mode.html) | `fun setDesktopMode(enable: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, reload: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.setDesktopMode](#) |
| [stopLoading](stop-loading.html) | `fun stopLoading(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [EngineSession.stopLoading](#) |

