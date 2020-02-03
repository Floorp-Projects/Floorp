[android-components](../index.md) / [mozilla.components.feature.addons.ui](./index.md)

## Package mozilla.components.feature.addons.ui

### Types

| Name | Summary |
|---|---|
| [AddonPermissionsAdapter](-addon-permissions-adapter/index.md) | `class AddonPermissionsAdapter : Adapter<ViewHolder>`<br>An adapter for displaying the permissions of an add-on. |
| [AddonsManagerAdapter](-addons-manager-adapter/index.md) | `class AddonsManagerAdapter : Adapter<`[`CustomViewHolder`](-custom-view-holder/index.md)`>`<br>An adapter for displaying add-on items. This will display information related to the state of an add-on such as recommended, unsupported or installed. In addition, it will perform actions such as installing an add-on. |
| [AddonsManagerAdapterDelegate](-addons-manager-adapter-delegate/index.md) | `interface AddonsManagerAdapterDelegate`<br>Provides methods for handling the add-on items in the add-on manager. |
| [CustomViewHolder](-custom-view-holder/index.md) | `sealed class CustomViewHolder : ViewHolder`<br>A base view holder. |
| [PermissionsDialogFragment](-permissions-dialog-fragment/index.md) | `class PermissionsDialogFragment : AppCompatDialogFragment`<br>A dialog that shows a set of permission required by an [Addon](../mozilla.components.feature.addons/-addon/index.md). |
| [UnsupportedAddonsAdapter](-unsupported-addons-adapter/index.md) | `class UnsupportedAddonsAdapter : Adapter<ViewHolder>`<br>An adapter for displaying unsupported add-on items. |
| [UnsupportedAddonsAdapterDelegate](-unsupported-addons-adapter-delegate/index.md) | `interface UnsupportedAddonsAdapterDelegate`<br>Provides methods for handling the success and error callbacks from uninstalling an add-on in the list of unsupported add-on items. |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [kotlin.collections.Map](kotlin.collections.-map/index.md) |  |

### Properties

| Name | Summary |
|---|---|
| [translatedName](translated-name.md) | `val `[`Addon`](../mozilla.components.feature.addons/-addon/index.md)`.translatedName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A shortcut to get the localized name of an add-on. |
