[android-components](../../../index.md) / [mozilla.components.feature.p2p.view](../../index.md) / [P2PView](../index.md) / [Listener](./index.md)

# Listener

`interface Listener` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PView.kt#L119)

An interface enabling the [P2PView](../index.md) to make requests of a controller.

### Functions

| Name | Summary |
|---|---|
| [onAccept](on-accept.md) | `abstract fun onAccept(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a decision to accept the connection specified by the given token. The value of the token is what was passed to [authenticate](../authenticate.md). |
| [onAdvertise](on-advertise.md) | `abstract fun onAdvertise(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to advertise the device's presence and desire to connect. |
| [onCloseToolbar](on-close-toolbar.md) | `abstract fun onCloseToolbar(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to close the toolbar. |
| [onDiscover](on-discover.md) | `abstract fun onDiscover(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to discovery other devices wishing to connect. |
| [onLoadData](on-load-data.md) | `abstract fun onLoadData(context: <ERROR CLASS>, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Loads the specified data into this tab. |
| [onReject](on-reject.md) | `abstract fun onReject(token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a decision to reject the connection specified by the given token. The value of the token is what was passed to [authenticate](../authenticate.md). |
| [onReset](on-reset.md) | `abstract fun onReset(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resets the connection to the neighbor. |
| [onSendPage](on-send-page.md) | `abstract fun onSendPage(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to send the current page to the neighbor. |
| [onSendUrl](on-send-url.md) | `abstract fun onSendUrl(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to send the current page's URL to the neighbor. |
| [onSetUrl](on-set-url.md) | `abstract fun onSetUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, newTab: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles a request to load the specified URL. This will typically be one sent from a neighbor. |

### Inheritors

| Name | Summary |
|---|---|
| [P2PInteractor](../../../mozilla.components.feature.p2p.internal/-p2-p-interactor/index.md) | `class P2PInteractor : `[`Listener`](./index.md)<br>Handle requests from [P2PView](../index.md), including interactions with the extension. |
