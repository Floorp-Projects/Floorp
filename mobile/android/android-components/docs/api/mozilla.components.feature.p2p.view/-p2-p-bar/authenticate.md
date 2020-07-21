[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PBar](index.md) / [authenticate](./authenticate.md)

# authenticate

`fun authenticate(neighborId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, neighborName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, token: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L88)

Overrides [P2PView.authenticate](../-p2-p-view/authenticate.md)

Handles authentication information about a connection. It is recommended that the view
prompt the user as to whether to connect to another device having the specified connection
information. It is highly recommended that the [token](../-p2-p-view/authenticate.md#mozilla.components.feature.p2p.view.P2PView$authenticate(kotlin.String, kotlin.String, kotlin.String)/token) be displayed, since it uniquely
identifies the connection and cannot be forged.

### Parameters

`neighborId` - a machine-generated ID uniquely identifying the other device

`neighborName` - a human-readable name of the other device

`token` - a short string of characters shared between the two devices