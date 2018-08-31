---
title: Toolbar - 
---

[mozilla.components.concept.toolbar](../index.html) / [Toolbar](./index.html)

# Toolbar

`interface Toolbar`

Interface to be implemented by components that provide browser toolbar functionality.

### Types

| [Action](-action/index.html) | `interface Action`<br>Generic interface for actions to be added to the toolbar. |
| [ActionButton](-action-button/index.html) | `open class ActionButton : `[`Action`](-action/index.html)<br>An action button to be added to the toolbar. |
| [ActionImage](-action-image/index.html) | `open class ActionImage : `[`Action`](-action/index.html)<br>An action that just shows a static, non-clickable image. |
| [ActionSpace](-action-space/index.html) | `open class ActionSpace : `[`Action`](-action/index.html)<br>An "empty" action with a desired width to be used as "placeholder". |
| [ActionToggleButton](-action-toggle-button/index.html) | `open class ActionToggleButton : `[`Action`](-action/index.html)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |

### Properties

| [url](url.html) | `abstract var url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets/Gets the URL to be displayed on the toolbar. |

### Functions

| [addBrowserAction](add-browser-action.html) | `abstract fun addBrowserAction(action: `[`Action`](-action/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the toolbar in display mode. |
| [addNavigationAction](add-navigation-action.html) | `abstract fun addNavigationAction(action: `[`Action`](-action/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the far left side of the URL in display mode. |
| [addPageAction](add-page-action.html) | `abstract fun addPageAction(action: `[`Action`](-action/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the URL in display mode. |
| [asView](as-view.html) | `open fun asView(): View`<br>Casts this toolbar to an Android View object. |
| [displayProgress](display-progress.html) | `abstract fun displayProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given loading progress. Expects values in the range [0,100](#). |
| [onBackPressed](on-back-pressed.html) | `abstract fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Should be called by an activity when the user pressed the back key of the device. |
| [setOnUrlCommitListener](set-on-url-commit-listener.html) | `abstract fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when the user selected a new URL i.e. is done editing. |
| [setSearchTerms](set-search-terms.html) | `abstract fun setSearchTerms(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the currently used search terms as part of this Toolbar. |

