[android-components](../../index.md) / [mozilla.components.support.images](../index.md) / [CancelOnDetach](./index.md)

# CancelOnDetach

`class CancelOnDetach` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/images/src/main/java/mozilla/components/support/images/CancelOnDetach.kt#L13)

Cancels the provided job when a view is detached from the window

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CancelOnDetach(job: Job)`<br>Cancels the provided job when a view is detached from the window |

### Functions

| Name | Summary |
|---|---|
| [onViewAttachedToWindow](on-view-attached-to-window.md) | `fun onViewAttachedToWindow(v: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onViewDetachedFromWindow](on-view-detached-from-window.md) | `fun onViewDetachedFromWindow(v: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
