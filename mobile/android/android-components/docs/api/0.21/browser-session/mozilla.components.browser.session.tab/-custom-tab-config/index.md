---
title: CustomTabConfig - 
---

[mozilla.components.browser.session.tab](../index.html) / [CustomTabConfig](./index.html)

# CustomTabConfig

`class CustomTabConfig`

Holds configuration data for a Custom Tab. Use [createFromIntent](create-from-intent.html) to
create instances.

### Properties

| [actionButtonConfig](action-button-config.html) | `val actionButtonConfig: `[`CustomTabActionButtonConfig`](../-custom-tab-action-button-config/index.html)`?` |
| [closeButtonIcon](close-button-icon.html) | `val closeButtonIcon: Bitmap?` |
| [disableUrlbarHiding](disable-urlbar-hiding.html) | `val disableUrlbarHiding: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [id](id.html) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [menuItems](menu-items.html) | `val menuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`CustomTabMenuItem`](../-custom-tab-menu-item/index.html)`>` |
| [options](options.html) | `val options: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>` |
| [showShareMenuItem](show-share-menu-item.html) | `val showShareMenuItem: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [toolbarColor](toolbar-color.html) | `val toolbarColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` |

### Companion Object Functions

| [createFromIntent](create-from-intent.html) | `fun createFromIntent(intent: SafeIntent): `[`CustomTabConfig`](./index.md)<br>Creates a CustomTabConfig instance based on the provided intent. |
| [isCustomTabIntent](is-custom-tab-intent.html) | `fun isCustomTabIntent(intent: SafeIntent): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the provided intent is a custom tab intent. |

