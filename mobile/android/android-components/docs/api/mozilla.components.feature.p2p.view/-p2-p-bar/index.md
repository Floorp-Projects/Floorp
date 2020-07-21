[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PBar](./index.md)

# P2PBar

`class P2PBar : ConstraintLayout, `[`P2PView`](../-p2-p-view/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L24)

A toolbar for peer-to-peer communication between browsers. Setting [listener](listener.md) causes the
buttons to become active.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `P2PBar(context: <ERROR CLASS>, attrs: <ERROR CLASS>? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A toolbar for peer-to-peer communication between browsers. Setting [listener](listener.md) causes the buttons to become active. |

### Properties

| Name | Summary |
|---|---|
| [listener](listener.md) | `var listener: `[`Listener`](../-p2-p-view/-listener/index.md)`?`<br>Listener to be invoked after the user performs certain actions, such as initiating or accepting a connection. |

### Functions

| Name | Summary |
|---|---|
| [authenticate](authenticate.md) | `fun authenticate(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles authentication information about a connection. It is recommended that the view prompt the user as to whether to connect to another device having the specified connection information. It is highly recommended that the [token](../-p2-p-view/authenticate.md#mozilla.components.feature.p2p.view.P2PView$authenticate(kotlin.String, kotlin.String, kotlin.String)/token) be displayed, since it uniquely identifies the connection and cannot be forged. |
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clears the UI state. |
| [failure](failure.md) | `fun failure(msg: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicates that a failure state was entered. |
| [initializeButtons](initialize-buttons.md) | `fun initializeButtons(connectB: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, sendB: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>This initializes the buttons of a [P2PView](../-p2-p-view/index.md) to restore the past button state. It need not be called on the first [P2PView](../-p2-p-view/index.md) but should be called after one is destroyed and another inflated. It is up to the implementation how the buttons should be presented (such as whether they should be [View.GONE](#), [View.INVISIBLE](#), or just disabled when not being presented. The reset button is enabled whenever the connection buttons are not. |
| [readyToSend](ready-to-send.md) | `fun readyToSend(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicates that data can be sent to the connected device. |
| [receivePage](receive-page.md) | `fun receivePage(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, page: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles receipt of a web page from another device. Only an internal error would cause `neighborName` to be null. To be robust, a view should use `neighborName ?: neighborId` as the name of the neighbor. |
| [receiveUrl](receive-url.md) | `fun receiveUrl(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handles receipt of a URL from the specified neighbor. For example, the view could prompt the user to accept the URL, upon which [Listener.onSetUrl](../-p2-p-view/-listener/on-set-url.md) would be called. Only an internal error would cause `neighborName` to be null. To be robust, a view should use `neighborName ?: neighborId` as the name of the neighbor. |
| [reportSendComplete](report-send-complete.md) | `fun reportSendComplete(resid: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Indicates that the requested send operation is complete. The view may optionally display the specified string. |
| [updateStatus](update-status.md) | `fun updateStatus(status: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Optionally displays the status of the connection. The implementation should not take other actions based on the status, since the values of the strings could change.`fun updateStatus(id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Optionally displays the status of the connection. The implementation should not take other actions based on the id or corresponding string, since the values could change. |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../-p2-p-view/as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this [P2PView](../-p2-p-view/index.md) interface to an actual Android [View](#) object. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
