[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [Toolbar](./index.md)

# Toolbar

`interface Toolbar` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/Toolbar.kt#L23)

Interface to be implemented by components that provide browser toolbar functionality.

### Types

| Name | Summary |
|---|---|
| [Action](-action/index.md) | `interface Action`<br>Generic interface for actions to be added to the toolbar. |
| [ActionButton](-action-button/index.md) | `open class ActionButton : `[`Action`](-action/index.md)<br>An action button to be added to the toolbar. |
| [ActionImage](-action-image/index.md) | `open class ActionImage : `[`Action`](-action/index.md)<br>An action that just shows a static, non-clickable image. |
| [ActionSpace](-action-space/index.md) | `open class ActionSpace : `[`Action`](-action/index.md)<br>An "empty" action with a desired width to be used as "placeholder". |
| [ActionToggleButton](-action-toggle-button/index.md) | `open class ActionToggleButton : `[`Action`](-action/index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |
| [OnEditListener](-on-edit-listener/index.md) | `interface OnEditListener`<br>Listener to be invoked when the user edits the URL. |
| [SiteSecurity](-site-security/index.md) | `enum class SiteSecurity` |
| [SiteTrackingProtection](-site-tracking-protection/index.md) | `enum class SiteTrackingProtection`<br>Indicates which tracking protection status a site has. |

### Properties

| Name | Summary |
|---|---|
| [private](private.md) | `abstract var private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets/gets private mode. |
| [siteSecure](site-secure.md) | `abstract var siteSecure: `[`SiteSecurity`](-site-security/index.md)<br>Sets/Gets the site security to be displayed on the toolbar. |
| [siteTrackingProtection](site-tracking-protection.md) | `abstract var siteTrackingProtection: `[`SiteTrackingProtection`](-site-tracking-protection/index.md)<br>Sets/Gets the site tracking protection state to be displayed on the toolbar. |
| [title](title.md) | `abstract var title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets/Gets the title to be displayed on the toolbar. |
| [url](url.md) | `abstract var url: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)<br>Sets/Gets the URL to be displayed on the toolbar. |

### Functions

| Name | Summary |
|---|---|
| [addBrowserAction](add-browser-action.md) | `abstract fun addBrowserAction(action: `[`Action`](-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the toolbar in display mode. |
| [addEditAction](add-edit-action.md) | `abstract fun addEditAction(action: `[`Action`](-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed in edit mode. |
| [addNavigationAction](add-navigation-action.md) | `abstract fun addNavigationAction(action: `[`Action`](-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the far left side of the URL in display mode. |
| [addPageAction](add-page-action.md) | `abstract fun addPageAction(action: `[`Action`](-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the URL in display mode. |
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Casts this toolbar to an Android View object. |
| [displayMode](display-mode.md) | `abstract fun displayMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL displaying mode (from editing mode) if supported by the toolbar implementation. |
| [displayProgress](display-progress.md) | `abstract fun displayProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given loading progress. Expects values in the range [0,100](#). |
| [editMode](edit-mode.md) | `abstract fun editMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL editing mode (from displaying mode) if supported by the toolbar implementation. |
| [onBackPressed](on-back-pressed.md) | `abstract fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Should be called by an activity when the user pressed the back key of the device. |
| [onStop](on-stop.md) | `abstract fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Should be called by the host activity when it enters the stop state. |
| [setAutocompleteListener](set-autocomplete-listener.md) | `abstract fun setAutocompleteListener(filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when users changes text in the toolbar. |
| [setOnEditListener](set-on-edit-listener.md) | `abstract fun setOnEditListener(listener: `[`OnEditListener`](-on-edit-listener/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given listener to be invoked when the user edits the URL. |
| [setOnUrlCommitListener](set-on-url-commit-listener.md) | `abstract fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when the user selected a new URL i.e. is done editing. |
| [setSearchTerms](set-search-terms.md) | `abstract fun setSearchTerms(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the currently used search terms as part of this Toolbar. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserToolbar](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md) | `class BrowserToolbar : `[`Toolbar`](./index.md)<br>A customizable toolbar for browsers. |
