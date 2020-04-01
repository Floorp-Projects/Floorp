[android-components](../../index.md) / [mozilla.components.feature.p2p.view](../index.md) / [P2PBar](index.md) / [updateStatus](./update-status.md)

# updateStatus

`fun updateStatus(status: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L80)

Overrides [P2PView.updateStatus](../-p2-p-view/update-status.md)

Optionally displays the status of the connection. The implementation should not take other
actions based on the status, since the values of the strings could change.

### Parameters

`status` - the current status`fun updateStatus(id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/p2p/src/main/java/mozilla/components/feature/p2p/view/P2PBar.kt#L84)

Overrides [P2PView.updateStatus](../-p2-p-view/update-status.md)

Optionally displays the status of the connection. The implementation should not take other
actions based on the id or corresponding string, since the values could change.

### Parameters

`id` - the string resource id