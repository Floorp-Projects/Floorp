[android-components](../../index.md) / [mozilla.components.browser.toolbar](../index.md) / [BrowserToolbar](./index.md)

# BrowserToolbar

`class BrowserToolbar : `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`, `[`Toolbar`](../../mozilla.components.concept.toolbar/-toolbar/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/BrowserToolbar.kt#L67)

A customizable toolbar for browsers.

The toolbar can switch between two modes: display and edit. The display mode displays the current
URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
implemented by the DisplayToolbar and EditToolbar classes.

```
             +----------------+
             | BrowserToolbar |
             +--------+-------+
                      +
              +-------+-------+
              |               |
    +---------v------+ +-------v--------+
    | DisplayToolbar | |   EditToolbar  |
    +----------------+ +----------------+
```

### Types

| Name | Summary |
|---|---|
| [Button](-button/index.md) | `class Button : `[`ActionButton`](../../mozilla.components.concept.toolbar/-toolbar/-action-button/index.md)<br>An action button to be added to the toolbar. |
| [ToggleButton](-toggle-button/index.md) | `class ToggleButton : `[`ActionToggleButton`](../../mozilla.components.concept.toolbar/-toolbar/-action-toggle-button/index.md)<br>An action button with two states, selected and unselected. When the button is pressed, the state changes automatically. |
| [TwoStateButton](-two-state-button/index.md) | `class TwoStateButton : `[`Button`](-button/index.md)<br>An action that either shows an active button or an inactive button based on the provided isEnabled lambda. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `BrowserToolbar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, attrs: `[`AttributeSet`](https://developer.android.com/reference/android/util/AttributeSet.html)`? = null, defStyleAttr: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0)`<br>A customizable toolbar for browsers. |

### Properties

| Name | Summary |
|---|---|
| [browserActionMargin](browser-action-margin.md) | `var browserActionMargin: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets the margin to be used between browser actions. |
| [clearViewColor](clear-view-color.md) | `var clearViewColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets the color tint of the cancel button. |
| [displaySiteSecurityIcon](display-site-security-icon.md) | `var displaySiteSecurityIcon: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Set/Get whether a site security icon (usually a lock or globe icon) should be visible next to the URL. |
| [hint](hint.md) | `var hint: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets the text to be displayed when the URL of the toolbar is empty. |
| [hintColor](hint-color.md) | `var hintColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Sets the colour of the text to be displayed when the URL of the toolbar is empty. |
| [menuViewColor](menu-view-color.md) | `var menuViewColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets the color tint of the menu button. |
| [onUrlClicked](on-url-clicked.md) | `var onUrlClicked: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this lambda returns true the toolbar will switch to editing mode. Return false to not switch to editing mode and handle the click manually. |
| [private](private.md) | `var private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Sets/gets private mode. |
| [siteSecure](site-secure.md) | `var siteSecure: `[`SiteSecurity`](../../mozilla.components.concept.toolbar/-toolbar/-site-security/index.md)<br>Sets/Gets the site security to be displayed on the toolbar. |
| [siteSecurityColor](site-security-color.md) | `var siteSecurityColor: `[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`>`<br>Set/Get the site security icon colours (usually a lock or globe icon). It uses a pair of integers which represent the insecure and secure colours respectively. |
| [suggestionBackgroundColor](suggestion-background-color.md) | `var suggestionBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>The background color used for autocomplete suggestions in edit mode. |
| [suggestionForegroundColor](suggestion-foreground-color.md) | `var suggestionForegroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>The foreground color used for autocomplete suggestions in edit mode. |
| [textColor](text-color.md) | `var textColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Sets the colour of the text for the URL/search term displayed in the toolbar. |
| [textSize](text-size.md) | `var textSize: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)<br>Sets the size of the text for the URL/search term displayed in the toolbar. |
| [typeface](typeface.md) | `var typeface: `[`Typeface`](https://developer.android.com/reference/android/graphics/Typeface.html)<br>Sets the typeface of the text for the URL/search term displayed in the toolbar. |
| [url](url.md) | `var url: `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)<br>Sets/Gets the URL to be displayed on the toolbar. |
| [urlBoxMargin](url-box-margin.md) | `var urlBoxMargin: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Gets/Sets horizontal margin of the URL box (surrounding URL and page actions) in display mode. |
| [urlBoxView](url-box-view.md) | `var urlBoxView: `[`View`](https://developer.android.com/reference/android/view/View.html)`?`<br>Gets/Sets a custom view that will be drawn as behind the URL and page actions in display mode. |

### Functions

| Name | Summary |
|---|---|
| [addBrowserAction](add-browser-action.md) | `fun addBrowserAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the toolbar (outside of the URL bounding box) in display mode. |
| [addEditAction](add-edit-action.md) | `fun addEditAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right of the URL in edit mode. |
| [addNavigationAction](add-navigation-action.md) | `fun addNavigationAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be display on the far left side of the toolbar. This area is usually used on larger devices for navigation actions like "back" and "forward". |
| [addPageAction](add-page-action.md) | `fun addPageAction(action: `[`Action`](../../mozilla.components.concept.toolbar/-toolbar/-action/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds an action to be displayed on the right side of the URL in display mode. |
| [displayMode](display-mode.md) | `fun displayMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL displaying mode. |
| [displayProgress](display-progress.md) | `fun displayProgress(progress: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the given loading progress. Expects values in the range [0,100](#). |
| [editMode](edit-mode.md) | `fun editMode(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Switches to URL editing mode. |
| [invalidateActions](invalidate-actions.md) | `fun invalidateActions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Declare that the actions (navigation actions, browser actions, page actions) have changed and should be updated if needed. |
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Should be called by an activity when the user pressed the back key of the device. |
| [onLayout](on-layout.md) | `fun onLayout(changed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onMeasure](on-measure.md) | `fun onMeasure(widthMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, heightMeasureSpec: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [setAutocompleteListener](set-autocomplete-listener.md) | `fun setAutocompleteListener(filter: suspend (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`AutocompleteDelegate`](../../mozilla.components.concept.toolbar/-autocomplete-delegate/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when users changes text in the toolbar. |
| [setMenuBuilder](set-menu-builder.md) | `fun setMenuBuilder(menuBuilder: `[`BrowserMenuBuilder`](../../mozilla.components.browser.menu/-browser-menu-builder/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a BrowserMenuBuilder that will be used to create a menu when the menu button is clicked. The menu button will only be visible if a builder has been set. |
| [setOnEditFocusChangeListener](set-on-edit-focus-change-listener.md) | `fun setOnEditFocusChangeListener(listener: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when focus of the URL input view (in edit mode) changed. |
| [setOnEditListener](set-on-edit-listener.md) | `fun setOnEditListener(listener: `[`OnEditListener`](../../mozilla.components.concept.toolbar/-toolbar/-on-edit-listener/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given listener to be invoked when the user edits the URL. |
| [setOnSiteSecurityClickedListener](set-on-site-security-clicked-listener.md) | `fun setOnSiteSecurityClickedListener(listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets a listener to be invoked when the site security indicator icon is clicked. |
| [setOnUrlCommitListener](set-on-url-commit-listener.md) | `fun setOnUrlCommitListener(listener: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers the given function to be invoked when the user selected a new URL i.e. is done editing. |
| [setSearchTerms](set-search-terms.md) | `fun setSearchTerms(searchTerms: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the currently used search terms as part of this Toolbar. |
| [setUrlTextPadding](set-url-text-padding.md) | `fun setUrlTextPadding(left: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingLeft, top: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingTop, right: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingRight, bottom: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = displayToolbar.urlView.paddingBottom): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Sets the padding to be applied to the URL text (in display mode). |

### Inherited Functions

| Name | Summary |
|---|---|
| [asView](../../mozilla.components.concept.toolbar/-toolbar/as-view.md) | `open fun asView(): `[`View`](https://developer.android.com/reference/android/view/View.html)<br>Casts this toolbar to an Android View object. |

### Extension Properties

| Name | Summary |
|---|---|
| [isLTR](../../mozilla.components.support.ktx.android.view/android.view.-view/is-l-t-r.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isLTR: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Left to Right? |
| [isRTL](../../mozilla.components.support.ktx.android.view/android.view.-view/is-r-t-l.md) | `val `[`View`](https://developer.android.com/reference/android/view/View.html)`.isRTL: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is the horizontal layout direction of this view from Right to Left? |

### Extension Functions

| Name | Summary |
|---|---|
| [forEach](../../mozilla.components.support.ktx.android.view/android.view.-view-group/for-each.md) | `fun `[`ViewGroup`](https://developer.android.com/reference/android/view/ViewGroup.html)`.forEach(action: (`[`View`](https://developer.android.com/reference/android/view/View.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Performs the given action on each View in this ViewGroup. |
| [hideKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/hide-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.hideKeyboard(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the soft input window. |
| [isGone](../../mozilla.components.support.ktx.android.view/android.view.-view/is-gone.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isGone(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.GONE. |
| [isInvisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-invisible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isInvisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.INVISIBLE. |
| [isVisible](../../mozilla.components.support.ktx.android.view/android.view.-view/is-visible.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.isVisible(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if this view's visibility is set to View.VISIBLE. |
| [setPadding](../../mozilla.components.support.ktx.android.view/android.view.-view/set-padding.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.setPadding(padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set a padding using [Padding](../../mozilla.components.support.base.android/-padding/index.md) object. |
| [showKeyboard](../../mozilla.components.support.ktx.android.view/android.view.-view/show-keyboard.md) | `fun `[`View`](https://developer.android.com/reference/android/view/View.html)`.showKeyboard(flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = InputMethodManager.SHOW_IMPLICIT): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Tries to focus this view and show the soft input window for it. |
