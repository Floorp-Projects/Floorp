/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.View.NO_ID
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.forEach
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBehavior
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.browser.toolbar.edit.EditToolbar
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.concept.toolbar.Toolbar.Highlight
import mozilla.components.support.base.android.Padding
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.ui.autocomplete.AutocompleteView
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import mozilla.components.ui.autocomplete.OnFilterListener
import kotlin.coroutines.CoroutineContext

// This is used for truncating URLs to prevent extreme cases from
// slowing down UI rendering e.g. in case of a bookmarklet or a data URI.
// https://github.com/mozilla-mobile/android-components/issues/5249
const val MAX_URI_LENGTH = 25000

internal fun ImageView.setTintResource(@ColorRes tintColorResource: Int) {
    if (tintColorResource != NO_ID) {
        imageTintList = ContextCompat.getColorStateList(context, tintColorResource)
    }
}
/**
 * A customizable toolbar for browsers.
 *
 * The toolbar can switch between two modes: display and edit. The display mode displays the current
 * URL and controls for navigation. In edit mode the current URL can be edited. Those two modes are
 * implemented by the DisplayToolbar and EditToolbar classes.
 *
 * ```
 *           +----------------+
 *           | BrowserToolbar |
 *           +--------+-------+
 *                    +
 *            +-------+-------+
 *            |               |
 *  +---------v------+ +-------v--------+
 *  | DisplayToolbar | |   EditToolbar  |
 *  +----------------+ +----------------+
 * ```
 */
class BrowserToolbar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ViewGroup(context, attrs, defStyleAttr), Toolbar {
    private var state: State = State.DISPLAY
    private var searchTerms: String = ""
    private var urlCommitListener: ((String) -> Boolean)? = null

    /**
     * Toolbar in "display mode".
     */
    var display = DisplayToolbar(
        context,
        this,
        LayoutInflater.from(context).inflate(
            R.layout.mozac_browser_toolbar_displaytoolbar,
            this,
            false
        )
    )
        @VisibleForTesting(otherwise = PRIVATE) internal set

    /**
     * Toolbar in "edit mode".
     */
    var edit = EditToolbar(
        context,
        this,
        LayoutInflater.from(context).inflate(
            R.layout.mozac_browser_toolbar_edittoolbar,
            this,
            false
        )
    )
        @VisibleForTesting(otherwise = PRIVATE) internal set

    override var title: String
        get() = display.title
        set(value) { display.title = value }

    override var url: CharSequence
        get() = display.url.toString()
        set(value) {
            // We update the display toolbar immediately. We do not do that for the edit toolbar to not
            // mess with what the user is entering. Instead we will remember the value and update the
            // edit toolbar whenever we switch to it.
            display.url = value.take(MAX_URI_LENGTH)
        }

    override var siteSecure: Toolbar.SiteSecurity
        get() = display.siteSecurity
        set(value) { display.siteSecurity = value }

    override var highlight: Highlight = Highlight.NONE
        set(value) {
            if (field != value) {
                display.setHighlight(value)
                field = value
            }
        }

    override var siteTrackingProtection: Toolbar.SiteTrackingProtection =
        Toolbar.SiteTrackingProtection.OFF_GLOBALLY
        set(value) {
                if (field != value) {
                    display.setTrackingProtectionState(value)
                    field = value
                }
            }

    override var private: Boolean
        get() = edit.private
        set(value) { edit.private = value }

    /**
     * Registers the given listener to be invoked when the user edits the URL.
     */
    override fun setOnEditListener(listener: Toolbar.OnEditListener) {
        edit.editListener = listener
    }

    /**
     * Registers the given function to be invoked when users changes text in the toolbar.
     *
     * @param filter A function which will perform autocompletion and send results to [AutocompleteDelegate].
     */
    override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
        // Our 'filter' knows how to autocomplete, and the 'urlView' knows how to apply results of
        // autocompletion. Which gives us a lovely delegate chain!
        // urlView decides when it's appropriate to ask for autocompletion, and in turn we invoke
        // our 'filter' and send results back to 'urlView'.
        edit.setAutocompleteListener(filter)
    }

    init {
        addView(display.rootView)
        addView(edit.rootView)

        updateState(State.DISPLAY)
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        forEach { child ->
            child.layout(
                0 + paddingLeft,
                0 + paddingTop,
                paddingLeft + child.measuredWidth,
                paddingTop + child.measuredHeight
            )
        }
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // Our toolbar will always use the full width and a fixed height (default) or the provided
        // height if it's an exact value.
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY) {
            MeasureSpec.getSize(heightMeasureSpec)
        } else {
            resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_default_toolbar_height)
        }

        setMeasuredDimension(width, height)

        // Let the children measure themselves using our fixed size (with padding subtracted)
        val childWidth = width - paddingLeft - paddingRight
        val childHeight = height - paddingTop - paddingBottom

        val childWidthSpec = MeasureSpec.makeMeasureSpec(childWidth, MeasureSpec.EXACTLY)
        val childHeightSpec = MeasureSpec.makeMeasureSpec(childHeight, MeasureSpec.EXACTLY)

        forEach { child -> child.measure(childWidthSpec, childHeightSpec) }
    }

    override fun onBackPressed(): Boolean {
        if (state == State.EDIT) {
            displayMode()
            return true
        }
        return false
    }

    override fun onStop() {
        display.onStop()
    }

    override fun setSearchTerms(searchTerms: String) {
        if (state == State.EDIT) {
            edit.editSuggestion(searchTerms)
        }

        this.searchTerms = searchTerms
    }

    override fun displayProgress(progress: Int) {
        display.updateProgress(progress)
    }

    override fun setOnUrlCommitListener(listener: (String) -> Boolean) {
        this.urlCommitListener = listener
    }

    /**
     * Declare that the actions (navigation actions, browser actions, page actions) have changed and
     * should be updated if needed.
     *
     * The toolbar will call the <code>visible</code> lambda of every action to determine whether a
     * view for this action should be added or removed. Additionally <code>bind</code> will be
     * called on every visible action to update its view.
     */
    override fun invalidateActions() {
        display.invalidateActions()
        edit.invalidateActions()
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar (outside of the URL bounding
     * box) in display mode.
     *
     * If there is not enough room to show all icons then some icons may be moved to an overflow
     * menu.
     *
     * Related:
     * https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action
     */
    override fun addBrowserAction(action: Toolbar.Action) {
        display.addBrowserAction(action)
    }

    /**
     * Removes a previously added browser action (see [addBrowserAction]). If the provided
     * action was never added, this method has no effect.
     *
     * @param action the action to remove.
     */
    override fun removeBrowserAction(action: Toolbar.Action) {
        display.removeBrowserAction(action)
    }

    /**
     * Removes a previously added page action (see [addPageAction]). If the provided
     * action was never added, this method has no effect.
     *
     * @param action the action to remove.
     */
    override fun removePageAction(action: Toolbar.Action) {
        display.removePageAction(action)
    }

    /**
     * Adds an action to be displayed on the right side of the URL in display mode.
     *
     * Related:
     * https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Page_actions
     */
    override fun addPageAction(action: Toolbar.Action) {
        display.addPageAction(action)
    }

    /**
     * Adds an action to be display on the far left side of the toolbar. This area is usually used
     * on larger devices for navigation actions like "back" and "forward".
     */
    override fun addNavigationAction(action: Toolbar.Action) {
        display.addNavigationAction(action)
    }

    /**
     * Adds an action to be displayed on the right of the URL in edit mode.
     */
    override fun addEditAction(action: Toolbar.Action) {
        edit.addEditAction(action)
    }

    /**
     * Switches to URL editing mode.
     */
    override fun editMode() {
        val urlValue = if (searchTerms.isEmpty()) url else searchTerms
        edit.updateUrl(urlValue.toString(), false)
        updateState(State.EDIT)
        edit.focus()
        edit.selectAll()
    }

    /**
     * Switches to URL displaying mode.
     */
    override fun displayMode() {
        updateState(State.DISPLAY)
    }

    /**
     * Dismisses the display toolbar popup menu.
     */
    override fun dismissMenu() {
        display.views.menu.dismissMenu()
    }

    override fun enableScrolling() {
        // Behavior can be changed without us knowing. Not safe to use a memoized value.
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.enableScrolling()
        }
    }

    override fun disableScrolling() {
        // Behavior can be changed without us knowing. Not safe to use a memoized value.
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.disableScrolling()
        }
    }

    override fun expand() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.forceExpand(this@BrowserToolbar)
        }
    }

    override fun collapse() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.forceCollapse(this@BrowserToolbar)
        }
    }

    internal fun onUrlEntered(url: String) {
        if (urlCommitListener?.invoke(url) != false) {
            // Return to display mode if there's no urlCommitListener or if it returned true. This lets
            // the app control whether we should switch to display mode automatically.
            displayMode()
        }
    }

    private fun updateState(state: State) {
        this.state = state

        val (show, hide) = when (state) {
            State.DISPLAY -> {
                edit.stopEditing()
                Pair(display.rootView, edit.rootView)
            }
            State.EDIT -> {
                edit.startEditing()
                Pair(edit.rootView, display.rootView)
            }
        }

        show.visibility = View.VISIBLE
        hide.visibility = View.GONE
    }

    private enum class State {
        DISPLAY,
        EDIT
    }

    /**
     * An action button to be added to the toolbar.
     *
     * @param imageDrawable The drawable to be shown.
     * @param contentDescription The content description to use.
     * @param visible Lambda that returns true or false to indicate whether this button should be shown.
     * @param background A custom (stateful) background drawable resource to be used.
     * @param padding a custom [Padding] for this Button.
     * @param iconTintColorResource Optional ID of color resource to tint the icon.
     * @param longClickListener Callback that will be invoked whenever the button is long-pressed.
     * @param listener Callback that will be invoked whenever the button is pressed
     */
    @Suppress("LongParameterList")
    open class Button(
        imageDrawable: Drawable,
        contentDescription: String,
        visible: () -> Boolean = { true },
        @DrawableRes background: Int = 0,
        val padding: Padding = DEFAULT_PADDING,
        @ColorRes iconTintColorResource: Int = NO_ID,
        longClickListener: (() -> Unit)? = null,
        listener: () -> Unit
    ) : Toolbar.ActionButton(
        imageDrawable,
        contentDescription,
        visible,
        background,
        padding,
        iconTintColorResource,
        longClickListener,
        listener
    )

    /**
     * An action button with two states, selected and unselected. When the button is pressed, the
     * state changes automatically.
     *
     * @param image The drawable to be shown if the button is in unselected state.
     * @param imageSelected The drawable to be shown if the button is in selected state.
     * @param contentDescription The content description to use if the button is in unselected state.
     * @param contentDescriptionSelected The content description to use if the button is in selected state.
     * @param visible Lambda that returns true or false to indicate whether this button should be shown.
     * @param selected Sets whether this button should be selected initially.
     * @param background A custom (stateful) background drawable resource to be used.
     * @param padding a custom [Padding] for this Button.
     * @param listener Callback that will be invoked whenever the checked state changes.
     */
    open class ToggleButton(
        image: Drawable,
        imageSelected: Drawable,
        contentDescription: String,
        contentDescriptionSelected: String,
        visible: () -> Boolean = { true },
        selected: Boolean = false,
        @DrawableRes background: Int = 0,
        val padding: Padding = DEFAULT_PADDING,
        listener: (Boolean) -> Unit
    ) : Toolbar.ActionToggleButton(
        image,
        imageSelected,
        contentDescription,
        contentDescriptionSelected,
        visible,
        selected,
        background,
        padding,
        listener
    )

    /**
     * An action that either shows an active button or an inactive button based on the provided
     * <code>isInPrimaryState</code> lambda. All secondary characteristics default to their
     * corresponding primary.
     *
     * @param primaryImage: The drawable to be shown if the button is in the primary/enabled state
     * @param primaryContentDescription: The content description to use if the button is in the primary state.
     * @param secondaryImage: The drawable to be shown if the button is in the secondary/disabled state.
     * @param secondaryContentDescription: The content description to use if the button is in the secondary state.
     * @param isInPrimaryState: Lambda that returns whether this button should be in the primary or secondary state.
     * @param primaryImageTintResource: Optional ID of color resource to tint the icon in the primary state.
     * @param secondaryImageTintResource: ID of color resource to tint the icon in the secondary state.
     * @param disableInSecondaryState: Disable the button entirely when in the secondary state?
     * @param background A custom (stateful) background drawable resource to be used.
     * @param longClickListener Callback that will be invoked whenever the button is long-pressed.
     * @param listener Callback that will be invoked whenever the button is pressed.
     */
    @Suppress("LongParameterList")
    open class TwoStateButton(
        val primaryImage: Drawable,
        val primaryContentDescription: String,
        val secondaryImage: Drawable = primaryImage,
        val secondaryContentDescription: String = primaryContentDescription,
        val isInPrimaryState: () -> Boolean = { true },
        @ColorRes val primaryImageTintResource: Int = NO_ID,
        @ColorRes val secondaryImageTintResource: Int = primaryImageTintResource,
        val disableInSecondaryState: Boolean = true,
        background: Int = 0,
        longClickListener: (() -> Unit)? = null,
        listener: () -> Unit
    ) : BrowserToolbar.Button(
        primaryImage,
        primaryContentDescription,
        background = background,
        longClickListener = longClickListener,
        listener = listener,
    ) {
        var enabled: Boolean = false
            private set

        override fun bind(view: View) {
            enabled = isInPrimaryState.invoke()

            val button = view as ImageButton
            if (enabled) {
                button.setImageDrawable(primaryImage)
                button.contentDescription = primaryContentDescription
                button.setTintResource(primaryImageTintResource)
                button.isEnabled = true
            } else {
                button.setImageDrawable(secondaryImage)
                button.contentDescription = secondaryContentDescription
                button.setTintResource(secondaryImageTintResource)
                button.isEnabled = !disableInSecondaryState
            }
        }
    }

    companion object {
        internal const val ACTION_PADDING_DP = 16
        internal val DEFAULT_PADDING =
            Padding(ACTION_PADDING_DP, ACTION_PADDING_DP, ACTION_PADDING_DP, ACTION_PADDING_DP)
    }
}

/**
 * Wraps [filter] execution in a coroutine context, cancelling prior executions on every invocation.
 * [coroutineContext] must be of type that doesn't propagate cancellation of its children upwards.
 */
class AsyncFilterListener(
    private val urlView: AutocompleteView,
    override val coroutineContext: CoroutineContext,
    private val filter: suspend (String, AutocompleteDelegate) -> Unit,
    private val uiContext: CoroutineContext = Dispatchers.Main
) : OnFilterListener, CoroutineScope {
    override fun invoke(text: String) {
        // We got a new input, so whatever past autocomplete queries we still have running are
        // irrelevant. We cancel them, but do not depend on cancellation to take place.
        coroutineContext.cancelChildren()

        CoroutineScope(coroutineContext).launch {
            filter(text, AsyncAutocompleteDelegate(urlView, this, uiContext))
        }
    }
}

/**
 * An autocomplete delegate which is aware of its parent scope (to check for cancellations).
 * Responsible for processing autocompletion results and discarding stale results when [urlView] moved on.
 */
private class AsyncAutocompleteDelegate(
    private val urlView: AutocompleteView,
    private val parentScope: CoroutineScope,
    override val coroutineContext: CoroutineContext,
    private val logger: Logger = Logger("AsyncAutocompleteDelegate")
) : AutocompleteDelegate, CoroutineScope {
    override fun applyAutocompleteResult(result: AutocompleteResult, onApplied: () -> Unit) {
        // Bail out if we were cancelled already.
        if (!parentScope.isActive) {
            logger.debug("Autocomplete request cancelled. Discarding results.")
            return
        }

        // Process results on the UI dispatcher.
        CoroutineScope(coroutineContext).launch {
            // Ignore this result if the query is stale.
            if (result.input == urlView.originalText) {
                urlView.applyAutocompleteResult(
                    InlineAutocompleteEditText.AutocompleteResult(
                        text = result.text,
                        source = result.source,
                        totalItems = result.totalItems
                    )
                )
                onApplied()
            } else {
                logger.debug("Discarding stale autocomplete result.")
            }
        }
    }

    override fun noAutocompleteResult(input: String) {
        // Bail out if we were cancelled already.
        if (!parentScope.isActive) {
            logger.debug("Autocomplete request cancelled. Discarding 'noAutocompleteResult'.")
            return
        }

        // Process results on the UI thread.
        CoroutineScope(coroutineContext).launch {
            // Ignore this result if the query is stale.
            if (input == urlView.originalText) {
                urlView.noAutocompleteResult()
            } else {
                logger.debug("Discarding stale lack of autocomplete results.")
            }
        }
    }
}
