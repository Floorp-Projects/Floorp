[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PView](index.md) / [updateStatus](./update-status.md)

# updateStatus

`abstract fun updateStatus(status: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PView.kt#L39)

Optionally displays the status of the connection. The implementation should not take other
actions based on the status, since the values of the strings could change.

### Parameters

`status` - the current status`abstract fun updateStatus(id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PView.kt#L47)

Optionally displays the status of the connection. The implementation should not take other
actions based on the id or corresponding string, since the values could change.

### Parameters

`id` - the string resource id