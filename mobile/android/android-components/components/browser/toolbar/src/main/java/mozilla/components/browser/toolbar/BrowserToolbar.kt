/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.support.annotation.DrawableRes
import android.support.annotation.VisibleForTesting
import android.support.v13.view.inputmethod.EditorInfoCompat
import android.util.AttributeSet
import android.view.View
import android.view.View.OnFocusChangeListener
import android.view.ViewGroup
import android.widget.ImageButton
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.browser.toolbar.edit.EditToolbar
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.android.Padding
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.res.pxToDp
import mozilla.components.support.ktx.android.view.forEach
import mozilla.components.support.ktx.android.view.isVisible
import mozilla.components.ui.autocomplete.AutocompleteView
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText
import mozilla.components.ui.autocomplete.OnFilterListener
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

private const val AUTOCOMPLETE_QUERY_THREADS = 3

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
@Suppress("TooManyFunctions")
class BrowserToolbar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ViewGroup(context, attrs, defStyleAttr), Toolbar {
    private val logger = Logger("BrowserToolbar")

    // displayToolbar and editToolbar are only visible internally and mutable so that we can mock
    // them in tests.
    @VisibleForTesting internal var displayToolbar = DisplayToolbar(context, this)
    @VisibleForTesting internal var editToolbar = EditToolbar(context, this)

    private val autocompleteExceptionHandler = CoroutineExceptionHandler { _, throwable ->
        logger.error("Error while processing autocomplete input", throwable)
    }

    private val autocompleteSupervisorJob = SupervisorJob()
    private val autocompleteDispatcher = autocompleteSupervisorJob +
        Executors.newFixedThreadPool(AUTOCOMPLETE_QUERY_THREADS).asCoroutineDispatcher() +
        autocompleteExceptionHandler

    /**
     * Sets/gets private mode.
     *
     * In private mode the IME should not update any personalized data such as typing history and personalized language
     * model based on what the user typed.
     */
    override var private: Boolean
        get() = (editToolbar.urlView.imeOptions and EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0
        set(value) {
            editToolbar.urlView.imeOptions = if (value) {
                editToolbar.urlView.imeOptions or EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING
            } else {
                editToolbar.urlView.imeOptions and (EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING.inv())
            }
        }

    /**
     * Set/Get whether a site security icon (usually a lock or globe icon) should be visible next to the URL.
     */
    var displaySiteSecurityIcon: Boolean
        get() = displayToolbar.siteSecurityIconView.isVisible()
        set(value) {
            displayToolbar.siteSecurityIconView.visibility = if (value) View.VISIBLE else View.GONE
        }

    /**
     *  Set/Get the site security icon colours (usually a lock or globe icon). It uses a pair of integers
     *  which represent the insecure and secure colours respectively.
     */
    var siteSecurityColor: Pair<Int, Int>
        get() = displayToolbar.securityIconColor
        set(value) { displayToolbar.securityIconColor = value }

    /**
     * Gets/Sets a custom view that will be drawn as behind the URL and page actions in display mode.
     */
    var urlBoxView: View?
        get() = displayToolbar.urlBoxView
        set(value) { displayToolbar.urlBoxView = value }

    /**
     * Gets/Sets the color tint of the menu button.
     */
    var menuViewColor: Int
        get() = displayToolbar.menuViewColor
        set(value) { displayToolbar.menuViewColor = value }

    /**
     * Gets/Sets the color tint of the cancel button.
     */
    var clearViewColor: Int
        get() = editToolbar.clearViewColor
        set(value) { editToolbar.clearViewColor = value }

    /**
     * Gets/Sets the margin to be used between browser actions.
     */
    var browserActionMargin: Int
        get() = displayToolbar.browserActionMargin
        set(value) { displayToolbar.browserActionMargin = value }

    /**
     * Gets/Sets horizontal margin of the URL box (surrounding URL and page actions) in display mode.
     */
    var urlBoxMargin: Int
        get() = displayToolbar.urlBoxMargin
        set(value) { displayToolbar.urlBoxMargin = value }

    /**
     * Sets a lambda that will be invoked whenever the URL in display mode was clicked. Only if this
     * lambda returns <code>true</code> the toolbar will switch to editing mode. Return
     * <code>false</code> to not switch to editing mode and handle the click manually.
     */
    var onUrlClicked: () -> Boolean
        get() = displayToolbar.onUrlClicked
        set(value) { displayToolbar.onUrlClicked = value }

    /**
     * Sets the text to be displayed when the URL of the toolbar is empty.
     */
    var hint: String
        get() = displayToolbar.urlView.hint.toString()
        set(value) {
            displayToolbar.urlView.hint = value
            editToolbar.urlView.hint = value
        }

    /**
     * Sets the colour of the text to be displayed when the URL of the toolbar is empty.
     */
    var hintColor: Int
        get() = displayToolbar.urlView.currentHintTextColor
        set(value) {
            displayToolbar.urlView.setHintTextColor(value)
            editToolbar.urlView.setHintTextColor(value)
        }

    /**
     * Sets the colour of the text for the URL/search term displayed in the toolbar.
     */
    var textColor: Int
        get() = displayToolbar.urlView.currentTextColor
        set(value) {
            displayToolbar.urlView.setTextColor(value)
            editToolbar.urlView.setTextColor(value)
        }

    /**
     * Sets the size of the text for the URL/search term displayed in the toolbar.
     */
    var textSize: Float
        get() = displayToolbar.urlView.textSize
        set(value) {
            displayToolbar.urlView.textSize = value
            editToolbar.urlView.textSize = value
        }

    /**
     * The background color used for autocomplete suggestions in edit mode.
     */
    var suggestionBackgroundColor: Int
        get() = editToolbar.urlView.autoCompleteBackgroundColor
        set(value) { editToolbar.urlView.autoCompleteBackgroundColor = value }

    /**
     * The foreground color used for autocomplete suggestions in edit mode.
     */
    var suggestionForegroundColor: Int?
        get() = editToolbar.urlView.autoCompleteForegroundColor
        set(value) { editToolbar.urlView.autoCompleteForegroundColor = value }

    /**
     * Sets the typeface of the text for the URL/search term displayed in the toolbar.
     */
    var typeface: Typeface
        get() = displayToolbar.urlView.typeface
        set(value) {
            displayToolbar.urlView.typeface = value
            editToolbar.urlView.typeface = value
        }

    /**
     * Sets a listener to be invoked when focus of the URL input view (in edit mode) changed.
     */
    fun setOnEditFocusChangeListener(listener: (Boolean) -> Unit) {
        editToolbar.urlView.onFocusChangeListener = OnFocusChangeListener { _, hasFocus ->
            listener.invoke(hasFocus)
        }
    }

    override fun setOnEditListener(listener: Toolbar.OnEditListener) {
        editToolbar.editListener = listener
    }

    override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
        // Our 'filter' knows how to autocomplete, and the 'urlView' knows how to apply results of
        // autocompletion. Which gives us a lovely delegate chain!
        // urlView decides when it's appropriate to ask for autocompletion, and in turn we invoke
        // our 'filter' and send results back to 'urlView'.
        editToolbar.urlView.setOnFilterListener(
            AsyncFilterListener(editToolbar.urlView, autocompleteDispatcher, filter)
        )
    }

    /**
     * Sets the padding to be applied to the URL text (in display mode).
     */
    fun setUrlTextPadding(
        left: Int = displayToolbar.urlView.paddingLeft,
        top: Int = displayToolbar.urlView.paddingTop,
        right: Int = displayToolbar.urlView.paddingRight,
        bottom: Int = displayToolbar.urlView.paddingBottom
    ) = displayToolbar.urlView.setPadding(left, top, right, bottom)

    private var state: State = State.DISPLAY
    private var searchTerms: String = ""
    private var urlCommitListener: ((String) -> Boolean)? = null

    override var url: CharSequence = ""
        set(value) {
            // We update the display toolbar immediately. We do not do that for the edit toolbar to not
            // mess with what the user is entering. Instead we will remember the value and update the
            // edit toolbar whenever we switch to it.
            displayToolbar.updateUrl(value)

            field = value
        }

    override var siteSecure: Toolbar.SiteSecurity = Toolbar.SiteSecurity.INSECURE
        set(value) {
            displayToolbar.setSiteSecurity(value)
            field = value
        }

    init {
        context.obtainStyledAttributes(attrs, R.styleable.BrowserToolbar, defStyleAttr, 0).run {
            attrs?.let {
                hintColor = getColor(
                    R.styleable.BrowserToolbar_browserToolbarHintColor,
                    hintColor
                )
                textColor = getColor(
                    R.styleable.BrowserToolbar_browserToolbarTextColor,
                    textColor
                )
                textSize = getDimension(
                    R.styleable.BrowserToolbar_browserToolbarTextSize,
                    textSize
                ) / resources.displayMetrics.density
                menuViewColor = getColor(
                    R.styleable.BrowserToolbar_browserToolbarMenuColor,
                    displayToolbar.menuViewColor
                )
                clearViewColor = getColor(
                    R.styleable.BrowserToolbar_browserToolbarClearColor,
                    editToolbar.clearViewColor
                )
                if (peekValue(R.styleable.BrowserToolbar_browserToolbarSuggestionForegroundColor) != null) {
                    suggestionForegroundColor = getColor(
                        R.styleable.BrowserToolbar_browserToolbarSuggestionForegroundColor,
                        // Default color should not be used since we are checking for a value before using it.
                        Color.CYAN)
                }
                suggestionBackgroundColor = getColor(
                    R.styleable.BrowserToolbar_browserToolbarSuggestionBackgroundColor,
                    suggestionBackgroundColor
                )
                val inSecure = getColor(
                    R.styleable.BrowserToolbar_browserToolbarInsecureColor,
                    displayToolbar.securityIconColor.first
                )
                val secure = getColor(
                    R.styleable.BrowserToolbar_browserToolbarSecureColor,
                    displayToolbar.securityIconColor.second
                )
                siteSecurityColor = Pair(inSecure, secure)
                val fadingEdgeLength = getDimensionPixelSize(
                    R.styleable.BrowserToolbar_browserToolbarFadingEdgeSize,
                    resources.pxToDp(DisplayToolbar.URL_FADING_EDGE_SIZE_DP)
                )
                displayToolbar.urlView.setFadingEdgeLength(fadingEdgeLength)
                displayToolbar.urlView.isHorizontalFadingEdgeEnabled = fadingEdgeLength > 0
            }
            recycle()
        }
        addView(displayToolbar)
        addView(editToolbar)

        updateState(State.DISPLAY)
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        forEach { child ->
            child.layout(
                0 + paddingLeft,
                0 + paddingTop,
                paddingLeft + child.measuredWidth,
                paddingTop + child.measuredHeight)
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
            resources.pxToDp(DEFAULT_TOOLBAR_HEIGHT_DP)
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

    override fun setSearchTerms(searchTerms: String) {
        this.searchTerms = searchTerms
    }

    override fun displayProgress(progress: Int) {
        displayToolbar.updateProgress(progress)
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
    fun invalidateActions() {
        displayToolbar.invalidateActions()
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
        displayToolbar.addBrowserAction(action)
    }

    /**
     * Adds an action to be displayed on the right side of the URL in display mode.
     *
     * Related:
     * https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Page_actions
     */
    override fun addPageAction(action: Toolbar.Action) {
        displayToolbar.addPageAction(action)
    }

    /**
     * Adds an action to be display on the far left side of the toolbar. This area is usually used
     * on larger devices for navigation actions like "back" and "forward".
     */
    override fun addNavigationAction(action: Toolbar.Action) {
        displayToolbar.addNavigationAction(action)
    }

    /**
     * Switches to URL editing mode.
     */
    override fun editMode() {
        val urlValue = if (searchTerms.isEmpty()) url else searchTerms
        editToolbar.updateUrl(urlValue.toString())

        updateState(State.EDIT)

        editToolbar.focus()
    }

    /**
     * Switches to URL displaying mode.
     */
    override fun displayMode() {
        updateState(State.DISPLAY)
    }

    /**
     * Sets a BrowserMenuBuilder that will be used to create a menu when the menu button is clicked.
     * The menu button will only be visible if a builder has been set.
     */
    fun setMenuBuilder(menuBuilder: BrowserMenuBuilder) {
        displayToolbar.menuBuilder = menuBuilder
    }

    internal fun onUrlEntered(url: String) {
        if (urlCommitListener?.invoke(url) != false) {
            // Return to display mode if there's no urlCommitListener or if it returned true. This lets
            // the app control whether we should switch to display mode automatically.
            displayMode()
        }
    }

    internal fun onEditCancelled() {
        if (editToolbar.editListener?.onCancelEditing() != false) {
            displayMode()
        }
    }

    private fun updateState(state: State) {
        this.state = state

        val (show, hide) = when (state) {
            State.DISPLAY -> {
                editToolbar.editListener?.onStopEditing()
                Pair(displayToolbar, editToolbar)
            }
            State.EDIT -> {
                editToolbar.editListener?.onStartEditing()
                Pair(editToolbar, displayToolbar)
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
     * @param listener Callback that will be invoked whenever the button is pressed
     */
    open class Button(
        imageDrawable: Drawable,
        contentDescription: String,
        visible: () -> Boolean = { true },
        @DrawableRes background: Int = 0,
        val padding: Padding = DEFAULT_PADDING,
        listener: () -> Unit
    ) : Toolbar.ActionButton(imageDrawable, contentDescription, visible, background, padding, listener)

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
     * <code>isEnabled</code> lambda.
     *
     * @param enabledImage The drawable to be show if the button is in the enabled stated.
     * @param enabledContentDescription The content description to use if the button is in the enabled state.
     * @param disabledImage The drawable to be show if the button is in the disabled stated.
     * @param disabledContentDescription The content description to use if the button is in the enabled state.
     * @param isEnabled Lambda that returns true of false to indicate whether this button should be enabled/disabled.
     * @param background A custom (stateful) background drawable resource to be used.
     * @param listener Callback that will be invoked whenever the checked state changes.
     */
    open class TwoStateButton(
        private val enabledImage: Drawable,
        private val enabledContentDescription: String,
        private val disabledImage: Drawable,
        private val disabledContentDescription: String,
        private val isEnabled: () -> Boolean = { true },
        background: Int = 0,
        listener: () -> Unit
    ) : BrowserToolbar.Button(
        enabledImage,
        enabledContentDescription,
        listener = listener,
        background = background
    ) {
        var enabled: Boolean = false
            private set

        override fun bind(view: View) {
            enabled = isEnabled.invoke()

            val button = view as ImageButton

            if (enabled) {
                button.setImageDrawable(disabledImage)
                button.contentDescription = disabledContentDescription
            } else {
                button.setImageDrawable(enabledImage)
                button.contentDescription = enabledContentDescription
            }
        }
    }

    companion object {
        private const val DEFAULT_TOOLBAR_HEIGHT_DP = 56
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
class AsyncAutocompleteDelegate(
    private val urlView: AutocompleteView,
    private val parentScope: CoroutineScope,
    override val coroutineContext: CoroutineContext,
    private val logger: Logger = Logger("AsyncAutocompleteDelegate")
) : AutocompleteDelegate, CoroutineScope {
    override fun applyAutocompleteResult(result: AutocompleteResult) {
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
