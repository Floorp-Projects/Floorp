[android-components](../../index.md) / [mozilla.components.browser.session](../index.md) / [SessionManager](index.md) / [restore](./restore.md)

# restore

`fun restore(snapshot: `[`Snapshot`](-snapshot/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/SessionManager.kt#L92)

Restores sessions from the provided [Snapshot](-snapshot/index.md).
Notification behaviour is as follows:

* onSessionAdded notifications will not fire,
* onSessionSelected notification will fire exactly once if the snapshot isn't empty,
* once snapshot has been restored, and appropriate session has been selected, onSessionsRestored
notification will fire.

### Parameters

`snapshot` - A [Snapshot](-snapshot/index.md) which may be produced by [createSnapshot](create-snapshot.md).