[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [ToolbarPresenter](./index.md)

# ToolbarPresenter

`class ToolbarPresenter : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/toolbar/src/main/java/mozilla/components/feature/toolbar/ToolbarPresenter.kt#L21)

Presenter implementation for a toolbar implementation in order to update the toolbar whenever
the state of the selected session changes.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ToolbarPresenter(toolbar: `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, urlRenderConfiguration: `[`UrlRenderConfiguration`](../-toolbar-feature/-url-render-configuration/index.md)`? = null)`<br>Presenter implementation for a toolbar implementation in order to update the toolbar whenever the state of the selected session changes. |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [onAllSessionsRemoved](on-all-sessions-removed.md) | `fun onAllSessionsRemoved(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>All sessions have been removed. Note that this will callback will be invoked whenever removeAll() or removeSessions have been called on the SessionManager. This callback will NOT be invoked when just the last session has been removed by calling remove() on the SessionManager. |
| [onProgress](on-progress.md) | `fun onProgress(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSearch](on-search.md) | `fun onSearch(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSecurityChanged](on-security-changed.md) | `fun onSecurityChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, securityInfo: `[`SecurityInfo`](../../mozilla.components.browser.session/-session/-security-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onSessionRemoved](on-session-removed.md) | `fun onSessionRemoved(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The given session has been removed. |
| [onSessionSelected](on-session-selected.md) | `fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new session has been selected: Update toolbar to display data of new session. |
| [onTrackerBlocked](on-tracker-blocked.md) | `fun onTrackerBlocked(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, tracker: `[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`, all: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tracker`](../../mozilla.components.concept.engine.content.blocking/-tracker/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTrackerBlockingEnabledChanged](on-tracker-blocking-enabled-changed.md) | `fun onTrackerBlockingEnabledChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, blockingEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onUrlChanged](on-url-changed.md) | `fun onUrlChanged(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start presenter: Display data in toolbar. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
