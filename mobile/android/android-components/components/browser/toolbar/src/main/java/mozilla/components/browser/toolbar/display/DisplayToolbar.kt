/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.annotation.SuppressLint
import android.content.Context
import android.support.v4.content.ContextCompat
import android.support.v7.widget.AppCompatImageButton
import android.support.v7.widget.AppCompatImageView
import android.support.v7.widget.AppCompatTextView
import android.transition.TransitionManager
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.widget.ProgressBar
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.facts.emitOpenMenuFact
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.concept.toolbar.Toolbar.SiteSecurity
import mozilla.components.support.ktx.android.content.res.pxToDp
import mozilla.components.support.ktx.android.view.isVisible
import mozilla.components.ui.icons.R.drawable.mozac_ic_globe
import mozilla.components.ui.icons.R.drawable.mozac_ic_lock

/**
 * Sub-component of the browser toolbar responsible for displaying the URL and related controls.
 *
 * Structure:
 *   +-------------+------+-----------------------+----------+------+
 *   | navigation  | icon | url       [ page    ] | browser  | menu |
 *   |   actions   |      |           [ actions ] | actions  |      |
 *   +-------------+------+-----------------------+----------+------+
 *
 * Navigation actions (optional):
 *     A dynamic list of clickable icons usually used for navigation on larger devices
 *     (e.g. “back”/”forward” buttons.)
 *
 * Icon (optional):
 *     Site security indicator icon (e.g. “Lock” icon) that may show a doorhanger when clicked.
 *
 * URL:
 *     Section that displays the current URL (read-only)
 *
 * Page actions (optional):
 *     A dynamic list of clickable icons inside the URL section (e.g. “reader mode” icon)
 *
 * Browser actions (optional):
 *     A list of dynamic clickable icons on the toolbar (e.g. tabs tray button)
 *
 * Menu (optional):
 *     A button that shows an overflow menu when clicked (constructed using the browser-menu
 *     component)
 *
 * Progress (optional):
 *     (Not shown in diagram) A horizontal photon-style progress bar provided by the ui-progress component.
 *
 */
@SuppressLint("ViewConstructor") // This view is only instantiated in code
@Suppress("LargeClass")
internal class DisplayToolbar(
    context: Context,
    val toolbar: BrowserToolbar
) : ViewGroup(context) {
    internal var menuBuilder: BrowserMenuBuilder? = null
        set(value) {
            field = value
            menuView.visibility = if (value == null) View.GONE else View.VISIBLE
        }

    internal val siteSecurityIconView = AppCompatImageView(context).apply {
        val padding = resources.pxToDp(ICON_PADDING_DP)
        setPadding(padding, padding, padding, padding)

        setImageResource(mozac_ic_globe)

        // Avoiding text behind the icon being selectable. If the listener is not set
        // with a value or null text behind the icon can be selectable.
        // https://github.com/mozilla-mobile/reference-browser/issues/448
        setOnClickListener(null)
    }

    internal val urlView = AppCompatTextView(context).apply {
        id = R.id.mozac_browser_toolbar_url_view
        gravity = Gravity.CENTER_VERTICAL
        textSize = URL_TEXT_SIZE

        setSingleLine(true)
        isClickable = true
        isFocusable = true

        setOnClickListener {
            if (onUrlClicked()) {
                toolbar.editMode()
            }
        }
    }

    private val menuView = AppCompatImageButton(context).apply {
        val padding = resources.pxToDp(MENU_PADDING_DP)
        setPadding(padding, padding, padding, padding)

        setImageResource(mozilla.components.ui.icons.R.drawable.mozac_ic_menu)
        contentDescription = context.getString(R.string.mozac_browser_toolbar_menu_button)

        val outValue = TypedValue()
        context.theme.resolveAttribute(
                android.R.attr.selectableItemBackgroundBorderless,
                outValue,
                true)

        setBackgroundResource(outValue.resourceId)
        visibility = View.GONE

        setOnClickListener {
            menuBuilder?.build(context)?.show(
                anchor = this,
                orientation = BrowserMenu.determineMenuOrientation(toolbar))

            emitOpenMenuFact()
        }
    }

    private val progressView = ProgressBar(
        context, null, android.R.attr.progressBarStyleHorizontal
    ).apply {
        visibility = View.GONE
        setAccessibilityDelegate(object : View.AccessibilityDelegate() {
            override fun onInitializeAccessibilityEvent(host: View?, event: AccessibilityEvent?) {
                super.onInitializeAccessibilityEvent(host, event)
                if (event?.eventType == AccessibilityEvent.TYPE_VIEW_SCROLLED) {
                    // Populate the scroll event with the current progress.
                    // See accessibility note in `updateProgress()`.
                    event.scrollY = progress
                    event.maxScrollY = max
                }
            }
        })
    }

    private val browserActions: MutableList<DisplayAction> = mutableListOf()
    private val pageActions: MutableList<DisplayAction> = mutableListOf()
    private val navigationActions: MutableList<DisplayAction> = mutableListOf()

    // Margin between browser actions.
    internal var browserActionMargin = 0

    // Horizontal margin of URL Box (surrounding URL and page actions).
    internal var urlBoxMargin = 0

    // An optional view to be drawn behind the URL and page actions.
    internal var urlBoxView: View? = null
        set(value) {
            // Remove previous view from ViewGroup
            if (field != null) {
                removeView(field)
            }
            // Add new view to ViewGroup (at position 0 to be drawn *before* the URL and page actions)
            if (value != null) {
                addView(value, 0)
            }
            field = value
        }

    // Callback to determine whether to open edit mode or not.
    internal var onUrlClicked: () -> Boolean = { true }

    private val defaultColor = ContextCompat.getColor(context, R.color.photonWhite)

    internal var securityIconColor = Pair(defaultColor, defaultColor)

    internal var menuViewColor = defaultColor
        set(value) {
            field = value
            menuView.setColorFilter(value)
        }

    init {
        addView(siteSecurityIconView)
        addView(urlView)
        addView(menuView)
        addView(progressView)
    }

    /**
     * Updates the URL to be displayed.
     */
    fun updateUrl(url: CharSequence) {
        urlView.text = url
    }

    /**
     * Updates the progress to be displayed.
     *
     * Accessibility note:
     *     ProgressBars can be made accessible to TalkBack by setting `android:accessibilityLiveRegion`.
     *     They will emit TYPE_VIEW_SELECTED events. TalkBack will format those events into percentage
     *     announcements along with a pitch-change earcon. We are not using that feature here for
     *     several reasons:
     *     1. They are dispatched via a 200ms timeout. Since loading a page can be a short process,
     *        and since we only update the bar a handful of times, these events often never fire and
     *        they don't give the user a true sense of the progress.
     *     2. The last 100% event is dispatched after the view is hidden. This prevents the event
     *        from being fired, so the user never gets a "complete" event.
     *     3. Live regions in TalkBack have their role announced, so the user will hear
     *        "Progress bar, 25%". For a common feature like page load this is very chatty and unintuitive.
     *     4. We can provide custom strings instead of the less useful percentage utterance, but
     *        TalkBack will not play an earcon if an event has its own text.
     *
     *     For all those reasons, we are going another route here with a "loading" announcement
     *     when the progress bar first appears along with scroll events that have the same
     *     pitch-change earcon in TalkBack (although they are a bit louder). This gives a concise and
     *     consistent feedback to the user that they can depend on.
     *
     */
    fun updateProgress(progress: Int) {
        if (!progressView.isVisible() && progress > 0) {
            // Loading has just started, make visible and announce "loading" for accessibility.
            progressView.visibility = View.VISIBLE
            progressView.announceForAccessibility(context.getString(R.string.mozac_browser_toolbar_progress_loading))
        }

        progressView.progress = progress
        progressView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED)

        if (progress >= progressView.max) {
            // Loading is done, hide progress bar.
            progressView.visibility = View.GONE
        }
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar.
     */
    fun addBrowserAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        if (action.visible()) {
            action.createView(this).let {
                displayAction.view = it
                addView(it)
            }
        }

        browserActions.add(displayAction)
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar.
     */
    fun addPageAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        if (action.visible()) {
            action.createView(this).let {
                displayAction.view = it
                addView(it)
            }
        }

        pageActions.add(displayAction)
    }

    /**
     * Adds an action to be displayed on the far left side of the toolbar.
     */
    fun addNavigationAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        if (action.visible()) {
            action.createView(this).let {
                displayAction.view = it
                addView(it)
            }
        }

        navigationActions.add(displayAction)
    }

    /**
     * Sets the site's security icon as secure if true, else the regular globe.
     */
    fun setSiteSecurity(secure: SiteSecurity) {
        val (image, color) = when (secure) {
            SiteSecurity.INSECURE -> Pair(mozac_ic_globe, securityIconColor.first)
            SiteSecurity.SECURE -> Pair(mozac_ic_lock, securityIconColor.second)
        }
        siteSecurityIconView.apply {
            setImageResource(image)
            setColorFilter(color)
        }
    }

    /**
     * Declare that the actions (navigation actions, browser actions, page actions) have changed and
     * should be updated if needed.
     */
    fun invalidateActions() {
        TransitionManager.beginDelayedTransition(this)

        for (action in navigationActions + pageActions + browserActions) {
            val visible = action.actual.visible()

            if (!visible && action.view != null) {
                // Action should not be visible anymore. Remove view.
                removeView(action.view)
                action.view = null
            } else if (visible && action.view == null) {
                // Action should be visible. Add view for it.
                action.actual.createView(this).let {
                    action.view = it
                    addView(it)
                }
            }

            action.view?.let { action.actual.bind(it) }
        }
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // This toolbar is using the full size provided by the parent
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        val fixedHeightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)

        setMeasuredDimension(width, height)

        // The icon and menu fill the whole height and have a square shape
        val iconSize = height
        val squareSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        siteSecurityIconView.measure(squareSpec, squareSpec)
        menuView.measure(squareSpec, squareSpec)

        // Measure all actions and use the available height for determining the size (square shape)
        val navigationActionsWidth = measureActions(navigationActions, size = height)
        val browserActionsWidth = measureActions(browserActions, size = height)
        val pageActionsWidth = measureActions(pageActions, size = height)

        // The url uses whatever space is left. Subtract the icon and (optionally) the menu
        val menuWidth = if (menuView.isVisible()) height else 0
        val urlWidth = (width - iconSize - browserActionsWidth - pageActionsWidth -
            menuWidth - navigationActionsWidth - 2 * urlBoxMargin)
        val urlWidthSpec = MeasureSpec.makeMeasureSpec(urlWidth, MeasureSpec.EXACTLY)
        urlView.measure(urlWidthSpec, fixedHeightSpec)

        val progressHeightSpec = MeasureSpec.makeMeasureSpec(resources.pxToDp(PROGRESS_BAR_HEIGHT_DP),
                MeasureSpec.EXACTLY)
        progressView.measure(widthMeasureSpec, progressHeightSpec)

        urlBoxView?.let {
            val urlBoxWidthSpec = MeasureSpec.makeMeasureSpec(
                iconSize + urlWidth + pageActionsWidth - 2 * urlBoxMargin,
                MeasureSpec.EXACTLY
            )
            it.measure(urlBoxWidthSpec, fixedHeightSpec)
        }
    }

    /**
     * Measures a list of actions and returns the needed width.
     */
    private fun measureActions(actions: List<DisplayAction>, size: Int): Int {
        val sizeSpec = MeasureSpec.makeMeasureSpec(size, MeasureSpec.EXACTLY)

        return actions
            .asSequence()
            .mapNotNull { it.view }
            .map { view ->
                val widthSpec = if (view.minimumWidth > size) {
                    MeasureSpec.makeMeasureSpec(view.minimumWidth, MeasureSpec.EXACTLY)
                } else {
                    sizeSpec
                }

                view.measure(widthSpec, sizeSpec)
                size
            }
            .sum()
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        // First we layout the navigation actions if there are any:
        //   +-------------+------------------------------------------------+
        //   | navigation  |                                                |
        //   |   actions   |                                                |
        //   +-------------+------------------------------------------------+

        val navigationActionsWidth = navigationActions
            .asSequence()
            .mapNotNull { it.view }
            .fold(0) { usedWidth, view ->
                val viewLeft = usedWidth
                val viewRight = viewLeft + view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        // The icon is always on the far left side of the toolbar. We cam lay it out even if it's
        // not going to be displayed:
        //   +-------------+------+-----------------------------------------+
        //   | navigation  | icon |                                         |
        //   |   actions   |      |                                         |
        //   +-------------+------+-----------------------------------------+

        siteSecurityIconView.layout(
            navigationActionsWidth,
            0,
            navigationActionsWidth + siteSecurityIconView.measuredWidth,
            measuredHeight
        )

        // The menu is always on the far right side of the toolbar:
        //   +-------------+------+----------------------------------+------+
        //   | navigation  | icon |                                  | menu |
        //   |   actions   |      |                                  |      |
        //   +-------------+------+----------------------------------+------+

        val menuWidth = if (menuView.isVisible()) height else 0
        menuView.layout(measuredWidth - menuView.measuredWidth, 0, measuredWidth, measuredHeight)

        // Now we add browser actions from the left side of the menu to the right (in reversed order):
        //   +-------------+------+-----------------------+----------+------+
        //   | navigation  | icon |                       | browser  | menu |
        //   |   actions   |      |                       | actions  |      |
        //   +-------------+------+-----------------------+----------+------+

        val browserActionWidth = browserActions
            .mapNotNull { it.view }
            .reversed()
            .fold(0) { usedWidth, view ->
                val margin = if (usedWidth > 0) browserActionMargin else 0

                val viewRight = measuredWidth - usedWidth - menuWidth - margin
                val viewLeft = viewRight - view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth + margin
            }

        // After browser actions we add page actions from the right to the left (in reversed order)
        //   +-------------+------+-----------------------+----------+------+
        //   | navigation  | icon |           [ page    ] | browser  | menu |
        //   |   actions   |      |           [ actions ] | actions  |      |
        //   +-------------+------+-----------------------+----------+------+

        val pageActionsWidth = pageActions
            .mapNotNull { it.view }
            .reversed()
            .fold(0) { usedWidth, view ->
                val viewRight = measuredWidth - browserActionWidth - usedWidth - menuWidth - urlBoxMargin
                val viewLeft = viewRight - view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        // Finally the URL uses whatever space is left:
        //   +-------------+------+-----------------------+----------+------+
        //   | navigation  | icon | url       [ page    ] | browser  | menu |
        //   |   actions   |      |           [ actions ] | actions  |      |
        //   +-------------+------+-----------------------+----------+------+

        val iconWidth = if (siteSecurityIconView.isVisible()) siteSecurityIconView.measuredWidth else 0
        val urlLeft = navigationActionsWidth + iconWidth + urlBoxMargin
        urlView.layout(urlLeft, 0, urlLeft + urlView.measuredWidth, measuredHeight)

        // The progress bar is going to be drawn at the bottom of the toolbar:

        progressView.layout(0, measuredHeight - progressView.measuredHeight, measuredWidth, measuredHeight)

        // The URL box view (if exists) is positioned behind the icon, the url and page actions:

        urlBoxView?.let { view ->
            val urlBoxLeft = navigationActionsWidth + urlBoxMargin
            view.layout(urlBoxLeft, 0, urlBoxLeft + view.measuredWidth, measuredHeight)
        }
    }

    companion object {
        internal const val URL_FADING_EDGE_SIZE_DP = 24

        private const val ICON_PADDING_DP = 16
        private const val MENU_PADDING_DP = 16
        private const val URL_TEXT_SIZE = 15f
        private const val PROGRESS_BAR_HEIGHT_DP = 3
    }
}

/**
 * A wrapper helper to pair a Toolbar.Action with an optional View.
 */
private class DisplayAction(
    var actual: Toolbar.Action,
    var view: View? = null
)
