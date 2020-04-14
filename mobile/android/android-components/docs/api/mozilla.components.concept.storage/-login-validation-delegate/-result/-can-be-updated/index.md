[android-components](../../../../index.md) / [mozilla.components.concept.storage](../../../index.md) / [LoginValidationDelegate](../../index.md) / [Result](../index.md) / [CanBeUpdated](./index.md)

# CanBeUpdated

`data class CanBeUpdated : `[`Result`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L207)

Indicates that a matching [Login](../../../-login/index.md) was found in storage, and the [Login](../../../-login/index.md) can be used
to update its information.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CanBeUpdated(foundLogin: `[`Login`](../../../-login/index.md)`)`<br>Indicates that a matching [Login](../../../-login/index.md) was found in storage, and the [Login](../../../-login/index.md) can be used to update its information. |

### Properties

| Name | Summary |
|---|---|
| [foundLogin](found-login.md) | `val foundLogin: `[`Login`](../../../-login/index.md) |
