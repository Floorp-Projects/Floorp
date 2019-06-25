[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissions](./index.md)

# SitePermissions

`data class SitePermissions` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissions.kt#L15)

A site permissions and its state.

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissions(parcel: <ERROR CLASS>)``SitePermissions(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, location: `[`Status`](-status/index.md)` = NO_DECISION, notification: `[`Status`](-status/index.md)` = NO_DECISION, microphone: `[`Status`](-status/index.md)` = NO_DECISION, camera: `[`Status`](-status/index.md)` = NO_DECISION, bluetooth: `[`Status`](-status/index.md)` = NO_DECISION, localStorage: `[`Status`](-status/index.md)` = NO_DECISION, savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>A site permissions and its state. |

### Properties

| Name | Summary |
|---|---|
| [bluetooth](bluetooth.md) | `val bluetooth: `[`Status`](-status/index.md) |
| [camera](camera.md) | `val camera: `[`Status`](-status/index.md) |
| [localStorage](local-storage.md) | `val localStorage: `[`Status`](-status/index.md) |
| [location](location.md) | `val location: `[`Status`](-status/index.md) |
| [microphone](microphone.md) | `val microphone: `[`Status`](-status/index.md) |
| [notification](notification.md) | `val notification: `[`Status`](-status/index.md) |
| [origin](origin.md) | `val origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [savedAt](saved-at.md) | `val savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Functions

| Name | Summary |
|---|---|
| [describeContents](describe-contents.md) | `fun describeContents(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [writeToParcel](write-to-parcel.md) | `fun writeToParcel(parcel: <ERROR CLASS>, flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createFromParcel](create-from-parcel.md) | `fun createFromParcel(parcel: <ERROR CLASS>): `[`SitePermissions`](./index.md) |
| [newArray](new-array.md) | `fun newArray(size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`SitePermissions`](./index.md)`?>` |
