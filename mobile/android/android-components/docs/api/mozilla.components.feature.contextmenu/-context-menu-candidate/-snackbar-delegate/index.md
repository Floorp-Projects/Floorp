[android-components](../../../index.md) / [mozilla.components.feature.contextmenu](../../index.md) / [ContextMenuCandidate](../index.md) / [SnackbarDelegate](./index.md)

# SnackbarDelegate

`interface SnackbarDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuCandidate.kt#L401)

Delegate to display a snackbar.

### Functions

| Name | Summary |
|---|---|
| [show](show.md) | `abstract fun show(snackBarParentView: <ERROR CLASS>, text: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, duration: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, action: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, listener: (<ERROR CLASS>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays a snackbar. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultSnackbarDelegate](../../-default-snackbar-delegate/index.md) | `class DefaultSnackbarDelegate : `[`SnackbarDelegate`](./index.md)<br>Default implementation for [ContextMenuCandidate.SnackbarDelegate](./index.md). Will display a standard default Snackbar. |
