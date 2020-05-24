[android-components](../../../index.md) / [mozilla.components.feature.addons.update](../../index.md) / [AddonUpdater](../index.md) / [UpdateAttempt](./index.md)

# UpdateAttempt

`data class UpdateAttempt` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L143)

Represents an attempt to update an add-on.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateAttempt(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, date: `[`Date`](http://docs.oracle.com/javase/7/docs/api/java/util/Date.html)`, status: `[`Status`](../-status/index.md)`?)`<br>Represents an attempt to update an add-on. |

### Properties

| Name | Summary |
|---|---|
| [addonId](addon-id.md) | `val addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [date](date.md) | `val date: `[`Date`](http://docs.oracle.com/javase/7/docs/api/java/util/Date.html) |
| [status](status.md) | `val status: `[`Status`](../-status/index.md)`?` |

### Extension Functions

| Name | Summary |
|---|---|
| [showInformationDialog](../../../mozilla.components.feature.addons.ui/show-information-dialog.md) | `fun `[`UpdateAttempt`](./index.md)`.showInformationDialog(context: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Shows a dialog containing all the information related to the given [AddonUpdater.UpdateAttempt](./index.md). |
