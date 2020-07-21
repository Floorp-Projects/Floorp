[android-components](../../index.md) / [mozilla.components.feature.p2p.internal](../index.md) / [P2PInteractor](./index.md)

# P2PInteractor

`class P2PInteractor : `[`Listener`](../../mozilla.components.feature.p2p.view/-p2-p-view/-listener/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/internal/P2PInteractor.kt#L25)

Handle requests from [P2PView](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md), including interactions with the extension.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `P2PInteractor(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, view: `[`P2PView`](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md)`, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, sessionUseCases: `[`SessionUseCases`](../../mozilla.components.feature.session/-session-use-cases/index.md)`, sender: `[`P2PFeatureSender`](../../mozilla.components.feature.p2p/-p2-p-feature/-p2-p-feature-sender/index.md)`, onClose: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, connectionProvider: () -> `[`NearbyConnection`](../../mozilla.components.lib.nearby/-nearby-connection/index.md)`, outgoingMessages: `[`ConcurrentMap`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ConcurrentMap.html)`<`[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`>)`<br>Handle requests from [P2PView](../../mozilla.components.feature.p2p.view/-p2-p-view/index.md), including interactions with the extension. |

### Functions

| Name | Summary |
|---|---|
| [onAccept](on-accept.md) | `fun onAccept(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a decision to accept the connection specified by the given token. The value of the token is what was passed to [authenticate](../../mozilla.components.feature.p2p.view/-p2-p-view/authenticate.md). |
| [onAdvertise](on-advertise.md) | `fun onAdvertise(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to advertise the device's presence and desire to connect. |
| [onCloseToolbar](on-close-toolbar.md) | `fun onCloseToolbar(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to close the toolbar. |
| [onDiscover](on-discover.md) | `fun onDiscover(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to discovery other devices wishing to connect. |
| [onLoadData](on-load-data.md) | `fun onLoadData(context: <ERROR CLASS>, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the specified data into this tab. |
| [onReject](on-reject.md) | `fun onReject(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a decision to reject the connection specified by the given token. The value of the token is what was passed to [authenticate](../../mozilla.components.feature.p2p.view/-p2-p-view/authenticate.md). |
| [onReset](on-reset.md) | `fun onReset(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resets the connection to the neighbor. |
| [onSendPage](on-send-page.md) | `fun onSendPage(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to send the current page to the neighbor. |
| [onSendUrl](on-send-url.md) | `fun onSendUrl(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to send the current page's URL to the neighbor. |
| [onSetUrl](on-set-url.md) | `fun onSetUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to load the specified URL. This will typically be one sent from a neighbor. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [HTML_INDICATOR](-h-t-m-l_-i-n-d-i-c-a-t-o-r.md) | `const val HTML_INDICATOR: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html) |
| [URL_INDICATOR](-u-r-l_-i-n-d-i-c-a-t-o-r.md) | `const val URL_INDICATOR: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
