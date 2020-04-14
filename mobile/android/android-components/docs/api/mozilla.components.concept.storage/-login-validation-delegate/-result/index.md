[android-components](../../../index.md) / [mozilla.components.concept.storage](../../index.md) / [LoginValidationDelegate](../index.md) / [Result](./index.md)

# Result

`sealed class Result` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L196)

The result of validating a given [Login](../../-login/index.md) against currently stored [Login](../../-login/index.md)s.  This will
include whether it can be created, updated, or neither, along with an explanation of any errors.

### Types

| Name | Summary |
|---|---|
| [CanBeCreated](-can-be-created.md) | `object CanBeCreated : `[`Result`](./index.md)<br>Indicates that the [Login](../../-login/index.md) does not currently exist in the storage, and a new entry with its information can be made. |
| [CanBeUpdated](-can-be-updated/index.md) | `data class CanBeUpdated : `[`Result`](./index.md)<br>Indicates that a matching [Login](../../-login/index.md) was found in storage, and the [Login](../../-login/index.md) can be used to update its information. |
| [Error](-error/index.md) | `sealed class Error : `[`Result`](./index.md)<br>The [Login](../../-login/index.md) cannot be saved. |

### Inheritors

| Name | Summary |
|---|---|
| [CanBeCreated](-can-be-created.md) | `object CanBeCreated : `[`Result`](./index.md)<br>Indicates that the [Login](../../-login/index.md) does not currently exist in the storage, and a new entry with its information can be made. |
| [CanBeUpdated](-can-be-updated/index.md) | `data class CanBeUpdated : `[`Result`](./index.md)<br>Indicates that a matching [Login](../../-login/index.md) was found in storage, and the [Login](../../-login/index.md) can be used to update its information. |
| [Duplicate](-error/-duplicate.md) | `object Duplicate : `[`Result`](./index.md)<br>Indicates that a duplicate [Login](../../-login/index.md) was found in storage, we should not save it again. |
| [Error](-error/index.md) | `sealed class Error : `[`Result`](./index.md)<br>The [Login](../../-login/index.md) cannot be saved. |
