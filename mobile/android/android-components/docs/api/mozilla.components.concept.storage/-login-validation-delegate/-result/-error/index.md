[android-components](../../../../index.md) / [mozilla.components.concept.storage](../../../index.md) / [LoginValidationDelegate](../../index.md) / [Result](../index.md) / [Error](./index.md)

# Error

`sealed class Error : `[`Result`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L212)

The [Login](../../../-login/index.md) cannot be saved.

### Types

| Name | Summary |
|---|---|
| [Duplicate](-duplicate.md) | `object Duplicate : `[`Result`](../index.md)<br>Indicates that a duplicate [Login](../../../-login/index.md) was found in storage, we should not save it again. |
| [EmptyPassword](-empty-password.md) | `object EmptyPassword : `[`Error`](./index.md)<br>The passed [Login](../../../-login/index.md) had an empty password field, and so cannot be saved. |
| [GeckoError](-gecko-error/index.md) | `data class GeckoError : `[`Error`](./index.md)<br>Something went wrong in GeckoView. We have no way to handle this type of error. See [exception](-gecko-error/exception.md) for details. |

### Inheritors

| Name | Summary |
|---|---|
| [EmptyPassword](-empty-password.md) | `object EmptyPassword : `[`Error`](./index.md)<br>The passed [Login](../../../-login/index.md) had an empty password field, and so cannot be saved. |
| [GeckoError](-gecko-error/index.md) | `data class GeckoError : `[`Error`](./index.md)<br>Something went wrong in GeckoView. We have no way to handle this type of error. See [exception](-gecko-error/exception.md) for details. |
