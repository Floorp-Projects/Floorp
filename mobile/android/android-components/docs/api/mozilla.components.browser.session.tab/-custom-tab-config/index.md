[android-components](../../index.md) / [mozilla.components.browser.session.tab](../index.md) / [CustomTabConfig](./index.md)

# CustomTabConfig

`class CustomTabConfig` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/tab/CustomTabConfig.kt#L25)

Holds configuration data for a Custom Tab. Use [createFromIntent](create-from-intent.md) to
create instances.

### Properties

| Name | Summary |
|---|---|
| [actionButtonConfig](action-button-config.md) | `val actionButtonConfig: `[`CustomTabActionButtonConfig`](../-custom-tab-action-button-config/index.md)`?` |
| [closeButtonIcon](close-button-icon.md) | `val closeButtonIcon: <ERROR CLASS>?` |
| [disableUrlbarHiding](disable-urlbar-hiding.md) | `val disableUrlbarHiding: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [menuItems](menu-items.md) | `val menuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabMenuItem`](../-custom-tab-menu-item/index.md)`>` |
| [options](options.md) | `val options: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [showShareMenuItem](show-share-menu-item.md) | `val showShareMenuItem: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [toolbarColor](toolbar-color.md) | `val toolbarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createFromIntent](create-from-intent.md) | `fun createFromIntent(intent: `[`SafeIntent`](../../mozilla.components.support.utils/-safe-intent/index.md)`, displayMetrics: <ERROR CLASS>? = null): `[`CustomTabConfig`](./index.md)<br>Creates a CustomTabConfig instance based on the provided intent. |
| [isCustomTabIntent](is-custom-tab-intent.md) | `fun isCustomTabIntent(intent: `[`SafeIntent`](../../mozilla.components.support.utils/-safe-intent/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the provided intent is a custom tab intent. |
