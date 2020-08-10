[android-components](../../index.md) / [mozilla.components.feature.p2p](../index.md) / [P2PFeature](./index.md)

# P2PFeature

`class P2PFeature : `[`SelectionAwareSessionObserver`](../../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`PermissionsFeature`](../../mozilla.components.support.base.feature/-permissions-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/P2PFeature.kt#L36)

Feature implementation for peer-to-peer communication between browsers.

### Types

| Name | Summary |
|---|---|
| [P2PFeatureSender](-p2-p-feature-sender/index.md) | `inner class P2PFeatureSender`<br>A class able to request an encoding of the current web page. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `P2PFeature(view: `[`P2PView`](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, connectionProvider: () -> `[`NearbyConnection`](../../mozilla.components.lib.nearby/-nearby-connection/index.md)`, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, sessionUseCases: `[`SessionUseCases`](../../mozilla.components.feature.session/-session-use-cases/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)`, onClose: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for peer-to-peer communication between browsers. |

### Properties

| Name | Summary |
|---|---|
| [onNeedToRequestPermissions](on-need-to-request-permissions.md) | `val onNeedToRequestPermissions: `[`OnNeedToRequestPermissions`](../../mozilla.components.support.base.feature/-on-need-to-request-permissions.md)<br>A callback invoked when permissions need to be requested by the feature before it can complete its task. Once the request is completed, [onPermissionsResult](../../mozilla.components.support.base.feature/-permissions-feature/on-permissions-result.md) needs to be invoked. |

### Inherited Properties

| Name | Summary |
|---|---|
| [activeSession](../../mozilla.components.browser.session/-selection-aware-session-observer/active-session.md) | `open var activeSession: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`?`<br>the currently observed session |

### Functions

| Name | Summary |
|---|---|
| [onPermissionsResult](on-permissions-result.md) | `fun onPermissionsResult(permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>, grantResults: `[`IntArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int-array/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies the feature that a permission request was completed. The feature should react to this and complete its task. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stops the observer. |

### Inherited Functions

| Name | Summary |
|---|---|
| [observeFixed](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-fixed.md) | `fun observeFixed(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the specified session. |
| [observeIdOrSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md) | `fun observeIdOrSelected(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the session matching the [sessionId](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-id-or-selected.md#mozilla.components.browser.session.SelectionAwareSessionObserver$observeIdOrSelected(kotlin.String)/sessionId). If the session does not exist, then observe the selected session. |
| [observeSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/observe-selected.md) | `fun observeSelected(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Starts observing changes to the selected session (see [SessionManager.selectedSession](../../mozilla.components.browser.session/-session-manager/selected-session.md)). If a different session is selected the observer will automatically be switched over and only notified of changes to the newly selected session. |
| [onSessionSelected](../../mozilla.components.browser.session/-selection-aware-session-observer/on-session-selected.md) | `open fun onSessionSelected(session: `[`Session`](../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The selection has changed and the given session is now the selected session. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
