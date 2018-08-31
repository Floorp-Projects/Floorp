---
title: Session - 
---

[mozilla.components.browser.session](../index.html) / [Session](./index.html)

# Session

`class Session : Observable<`[`Observer`](-observer/index.html)`>`

Value type that represents the state of a browser session. Changes can be observed.

### Types

| [FindResult](-find-result/index.html) | `data class FindResult`<br>A value type representing a result of a "find in page" operation. |
| [Observer](-observer/index.html) | `interface Observer`<br>Interface to be implemented by classes that want to observe a session. |
| [SecurityInfo](-security-info/index.html) | `data class SecurityInfo`<br>A value type holding security information for a Session. |
| [Source](-source/index.html) | `enum class Source`<br>Represents the origin of a session to describe how and why it was created. |

### Constructors

| [&lt;init&gt;](-init-.html) | `Session(initialUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, source: `[`Source`](-source/index.html)` = Source.NONE, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), delegate: Observable<`[`Observer`](-observer/index.html)`> = ObserverRegistry())`<br>Value type that represents the state of a browser session. Changes can be observed. |

### Properties

| [canGoBack](can-go-back.html) | `var canGoBack: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go back to, otherwise false. |
| [canGoForward](can-go-forward.html) | `var canGoForward: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Navigation state, true if there's an history item to go forward to, otherwise false. |
| [customTabConfig](custom-tab-config.html) | `var customTabConfig: `[`CustomTabConfig`](../../mozilla.components.browser.session.tab/-custom-tab-config/index.html)`?`<br>Configuration data in case this session is used for a Custom Tab. |
| [download](download.html) | `var download: Consumable<`[`Download`](../-download/index.html)`>`<br>Last download request if it wasn't consumed by at least one observer. |
| [findResults](find-results.html) | `var findResults: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`FindResult`](-find-result/index.html)`>`<br>List of results of that latest "find in page" operation. |
| [hitResult](hit-result.html) | `var hitResult: Consumable<HitResult>`<br>The target of the latest long click operation. |
| [id](id.html) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [loading](loading.html) | `var loading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Loading state, true if this session's url is currently loading, otherwise false. |
| [private](private.html) | `val private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [progress](progress.html) | `var progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The progress loading the current URL. |
| [searchTerms](search-terms.html) | `var searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently / last used search terms. |
| [securityInfo](security-info.html) | `var securityInfo: `[`SecurityInfo`](-security-info/index.html)<br>Security information indicating whether or not the current session is for a secure URL, as well as the host and SSL certificate authority, if applicable. |
| [source](source.html) | `val source: `[`Source`](-source/index.html) |
| [title](title.html) | `var title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The title of the currently displayed website changed. |
| [trackerBlockingEnabled](tracker-blocking-enabled.html) | `var trackerBlockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Tracker blocking state, true if blocking trackers is enabled, otherwise false. |
| [trackersBlocked](trackers-blocked.html) | `var trackersBlocked: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>`<br>List of URIs that have been blocked in this session. |
| [url](url.html) | `var url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The currently loading or loaded URL. |

### Functions

| [equals](equals.html) | `fun equals(other: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`?): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [hashCode](hash-code.html) | `fun hashCode(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isCustomTabSession](is-custom-tab-session.html) | `fun isCustomTabSession(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not this session is used for a Custom Tab. |

