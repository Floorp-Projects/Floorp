[android-components](../index.md) / [mozilla.components.support.base.feature](./index.md)

## Package mozilla.components.support.base.feature

### Types

| Name | Summary |
|---|---|
| [BackHandler](-back-handler/index.md) | `interface BackHandler`<br>Generic interface for fragments, features and other components that want to handle 'back' button presses. |
| [LifecycleAwareFeature](-lifecycle-aware-feature/index.md) | `interface LifecycleAwareFeature : LifecycleObserver`<br>An interface for all entry points to feature components to implement in order to make them lifecycle aware. |
| [PermissionsFeature](-permissions-feature/index.md) | `interface PermissionsFeature`<br>Interface for features that need to request permissions from the user. |
| [ViewBoundFeatureWrapper](-view-bound-feature-wrapper/index.md) | `class ViewBoundFeatureWrapper<T : `[`LifecycleAwareFeature`](-lifecycle-aware-feature/index.md)`>`<br>Wrapper for [LifecycleAwareFeature](-lifecycle-aware-feature/index.md) instances that keep a strong references to a [View](#). This wrapper is helpful when the lifetime of the [View](#) may be shorter than the [Lifecycle](#) and you need to keep a reference to the [LifecycleAwareFeature](-lifecycle-aware-feature/index.md) that may outlive the [View](#). |

### Type Aliases

| Name | Summary |
|---|---|
| [OnNeedToRequestPermissions](-on-need-to-request-permissions.md) | `typealias OnNeedToRequestPermissions = (permissions: `[`Array`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-array/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
