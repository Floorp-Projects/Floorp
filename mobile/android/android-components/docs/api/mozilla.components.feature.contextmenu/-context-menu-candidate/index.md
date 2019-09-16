[android-components](../../index.md) / [mozilla.components.feature.contextmenu](../index.md) / [ContextMenuCandidate](./index.md)

# ContextMenuCandidate

`data class ContextMenuCandidate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/contextmenu/src/main/java/mozilla/components/feature/contextmenu/ContextMenuCandidate.kt#L27)

A candidate for an item to be displayed in the context menu.

### Types

| Name | Summary |
|---|---|
| [SnackbarDelegate](-snackbar-delegate/index.md) | `interface SnackbarDelegate`<br>Delegate to display a snackbar. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ContextMenuCandidate(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, showFor: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, action: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>A candidate for an item to be displayed in the context menu. |

### Properties

| Name | Summary |
|---|---|
| [action](action.md) | `val action: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>The action to be invoked once the user selects this item. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>A unique ID that will be used to uniquely identify the candidate that the user selected. |
| [label](label.md) | `val label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The label that will be displayed in the context menu |
| [showFor](show-for.md) | `val showFor: (`[`SessionState`](../../mozilla.components.browser.state.state/-session-state/index.md)`, `[`HitResult`](../../mozilla.components.concept.engine/-hit-result/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If this lambda returns true for a given [SessionState](../../mozilla.components.browser.state.state/-session-state/index.md) and [HitResult](../../mozilla.components.concept.engine/-hit-result/index.md) then it will be displayed in the context menu. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createCopyImageLocationCandidate](create-copy-image-location-candidate.md) | `fun createCopyImageLocationCandidate(context: <ERROR CLASS>, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Copy Image Location". |
| [createCopyLinkCandidate](create-copy-link-candidate.md) | `fun createCopyLinkCandidate(context: <ERROR CLASS>, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Copy Link". |
| [createOpenImageInNewTabCandidate](create-open-image-in-new-tab-candidate.md) | `fun createOpenImageInNewTabCandidate(context: <ERROR CLASS>, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Open Image in New Tab". |
| [createOpenInNewTabCandidate](create-open-in-new-tab-candidate.md) | `fun createOpenInNewTabCandidate(context: <ERROR CLASS>, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Open Link in New Tab". |
| [createOpenInPrivateTabCandidate](create-open-in-private-tab-candidate.md) | `fun createOpenInPrivateTabCandidate(context: <ERROR CLASS>, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Open Link in Private Tab". |
| [createSaveImageCandidate](create-save-image-candidate.md) | `fun createSaveImageCandidate(context: <ERROR CLASS>, contextMenuUseCases: `[`ContextMenuUseCases`](../-context-menu-use-cases/index.md)`): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Save image". |
| [createShareLinkCandidate](create-share-link-candidate.md) | `fun createShareLinkCandidate(context: <ERROR CLASS>): `[`ContextMenuCandidate`](./index.md)<br>Context Menu item: "Share Link". |
| [defaultCandidates](default-candidates.md) | `fun defaultCandidates(context: <ERROR CLASS>, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, contextMenuUseCases: `[`ContextMenuUseCases`](../-context-menu-use-cases/index.md)`, snackBarParentView: <ERROR CLASS>, snackbarDelegate: `[`SnackbarDelegate`](-snackbar-delegate/index.md)` = DefaultSnackbarDelegate()): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ContextMenuCandidate`](./index.md)`>`<br>Returns the default list of context menu candidates. |
