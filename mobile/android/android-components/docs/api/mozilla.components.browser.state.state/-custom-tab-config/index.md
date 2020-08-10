[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [CustomTabConfig](./index.md)

# CustomTabConfig

`data class CustomTabConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/CustomTabConfig.kt#L32)

Holds configuration data for a Custom Tab.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabConfig(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), toolbarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, closeButtonIcon: <ERROR CLASS>? = null, enableUrlbarHiding: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, actionButtonConfig: `[`CustomTabActionButtonConfig`](../-custom-tab-action-button-config/index.md)`? = null, showCloseButton: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, showShareMenuItem: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, menuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabMenuItem`](../-custom-tab-menu-item/index.md)`> = emptyList(), exitAnimations: <ERROR CLASS>? = null, navigationBarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, titleVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, sessionToken: CustomTabsSessionToken? = null, externalAppType: `[`ExternalAppType`](../-external-app-type/index.md)` = ExternalAppType.CUSTOM_TAB)`<br>Holds configuration data for a Custom Tab. |

### Properties

| Name | Summary |
|---|---|
| [actionButtonConfig](action-button-config.md) | `val actionButtonConfig: `[`CustomTabActionButtonConfig`](../-custom-tab-action-button-config/index.md)`?`<br>Custom action button on the toolbar. |
| [closeButtonIcon](close-button-icon.md) | `val closeButtonIcon: <ERROR CLASS>?`<br>Custom icon of the back button on the toolbar. |
| [enableUrlbarHiding](enable-urlbar-hiding.md) | `val enableUrlbarHiding: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Enables the toolbar to hide as the user scrolls down on the page. |
| [exitAnimations](exit-animations.md) | `val exitAnimations: <ERROR CLASS>?`<br>Bundle containing custom exit animations for the tab. |
| [externalAppType](external-app-type.md) | `val externalAppType: `[`ExternalAppType`](../-external-app-type/index.md)<br>How this custom tab is being displayed. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>a unique ID of this custom tab. |
| [menuItems](menu-items.md) | `val menuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabMenuItem`](../-custom-tab-menu-item/index.md)`>`<br>Custom overflow menu items. |
| [navigationBarColor](navigation-bar-color.md) | `val navigationBarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Background color for the navigation bar. |
| [sessionToken](session-token.md) | `val sessionToken: CustomTabsSessionToken?`<br>The token associated with the custom tab. |
| [showCloseButton](show-close-button.md) | `val showCloseButton: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Specifies whether the close button will be shown on the toolbar. |
| [showShareMenuItem](show-share-menu-item.md) | `val showShareMenuItem: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Specifies whether a default share button will be shown in the menu. |
| [titleVisible](title-visible.md) | `val titleVisible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether the title should be shown in the custom tab. |
| [toolbarColor](toolbar-color.md) | `val toolbarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Background color for the toolbar. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
