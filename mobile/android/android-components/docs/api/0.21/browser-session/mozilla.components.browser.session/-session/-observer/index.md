---
title: Session.Observer - 
---

[mozilla.components.browser.session](../../index.html) / [Session](../index.html) / [Observer](./index.html)

# Observer

`interface Observer`

Interface to be implemented by classes that want to observe a session.

### Functions

| [onCustomTabConfigChanged](on-custom-tab-config-changed.html) | `open fun onCustomTabConfigChanged(session: `[`Session`](../index.html)`, customTabConfig: `[`CustomTabConfig`](../../../mozilla.components.browser.session.tab/-custom-tab-config/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDownload](on-download.html) | `open fun onDownload(session: `[`Session`](../index.html)`, download: `[`Download`](../../-download/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onFindResult](on-find-result.html) | `open fun onFindResult(session: `[`Session`](../index.html)`, result: `[`FindResult`](../-find-result/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLoadingStateChanged](on-loading-state-changed.html) | `open fun onLoadingStateChanged(session: `[`Session`](../index.html)`, loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLongPress](on-long-press.html) | `open fun onLongPress(session: `[`Session`](../index.html)`, hitResult: HitResult): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onNavigationStateChanged](on-navigation-state-changed.html) | `open fun onNavigationStateChanged(session: `[`Session`](../index.html)`, canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onProgress](on-progress.html) | `open fun onProgress(session: `[`Session`](../index.html)`, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSearch](on-search.html) | `open fun onSearch(session: `[`Session`](../index.html)`, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSecurityChanged](on-security-changed.html) | `open fun onSecurityChanged(session: `[`Session`](../index.html)`, securityInfo: `[`SecurityInfo`](../-security-info/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTitleChanged](on-title-changed.html) | `open fun onTitleChanged(session: `[`Session`](../index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlocked](on-tracker-blocked.html) | `open fun onTrackerBlocked(session: `[`Session`](../index.html)`, blocked: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlockingEnabledChanged](on-tracker-blocking-enabled-changed.html) | `open fun onTrackerBlockingEnabledChanged(session: `[`Session`](../index.html)`, blockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onUrlChanged](on-url-changed.html) | `open fun onUrlChanged(session: `[`Session`](../index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

