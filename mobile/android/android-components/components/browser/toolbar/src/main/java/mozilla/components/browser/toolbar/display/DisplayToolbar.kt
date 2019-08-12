/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.Drawable
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.widget.ProgressBar
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageView
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.core.view.setPadding
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.internal.ActionWrapper
import mozilla.components.browser.toolbar.internal.invalidateActions
import mozilla.components.browser.toolbar.internal.measureActions
import mozilla.components.browser.toolbar.internal.wrapAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.concept.toolbar.Toolbar.SiteSecurity
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_FOR_A_SITE
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.OFF_GLOBALLY
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection.ON_TRACKERS_BLOCKED

/**
 * Sub-component of the browser toolbar responsible for displaying the URL and related controls.
 *
 * Structure:
 * ```
 *   +-------------+-------+-----------------------+----------+------+
 *   | navigation  | icons | url       [ page    ] | browser  | menu |
 *   |   actions   |       |           [ actions ] | actions  |      |
 *   +-------------+-------+-----------------------+----------+------+
 * ```
 *
 * Navigation actions (optional):
 *     A dynamic list of clickable icons usually used for navigation on larger devices
 *     (e.g. “back”/”forward” buttons.)
 *
 * Icons (optional):
 *     Tracking protection indicator icon (e.g. “shield” icon) that may show a doorhanger when clicked.
 *     Separator icon: a vertical line that separate the above and below icons.
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
@Suppress("ViewConstructor", "LargeClass", "TooManyFunctions")
internal class DisplayToolbar(
    context: Context,
    val toolbar: BrowserToolbar
) : ViewGroup(context) {
    internal var menuBuilder: BrowserMenuBuilder?
        get() = menuView.menuBuilder
        set(value) { menuView.menuBuilder = value }

    internal var displayTrackingProtectionIcon: Boolean = false
        set(value) {
            field = value
            setTrackingProtectionState(siteTrackingProtection)
        }

    private val browserActions: MutableList<ActionWrapper> = mutableListOf()
    private val pageActions: MutableList<ActionWrapper> = mutableListOf()
    private val navigationActions: MutableList<ActionWrapper> = mutableListOf()

    // Margin between browser actions.
    internal var browserActionMargin = 0

    // Location of progress bar
    internal var progressBarGravity = BOTTOM_PROGRESS_BAR

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

    @ColorInt
    internal val defaultColor = ContextCompat.getColor(context, R.color.photonWhite)

    @ColorInt
    internal var separatorColor = ContextCompat.getColor(context, R.color.photonGrey80)

    private var currentSiteSecurity = SiteSecurity.INSECURE

    private var siteTrackingProtection = OFF_GLOBALLY

    internal var securityIcon = context.getDrawable(R.drawable.mozac_ic_site_security)
        set(value) {
            field = value
            siteSecurityIconView.setImageDrawable(value)
        }

    internal var securityIconColor = defaultColor to defaultColor
        set(value) {
            field = value
            setSiteSecurity(currentSiteSecurity)
        }

    internal var menuViewColor = defaultColor
        set(value) {
            field = value
            menuView.setColorFilter(value)
        }

    internal val trackingProtectionIconView = TrackingProtectionIconView(context).apply {
        isVisible = false
        setImageResource(R.drawable.mozac_tracking_protection_state_list)
        setPadding(resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_icon_padding))
        // Avoiding text behind the icon being selectable. If the listener is not set
        // with a value or null text behind the icon can be selectable.
        setOnClickListener(null)
    }

    internal val trackingProtectionAndSecurityIndicatorSeparatorView =
        AppCompatImageView(context).apply {
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            isVisible = false

            setImageResource(R.drawable.mozac_browser_toolbar_icons_vertical_separator)

            // Avoiding text behind the icon being selectable. If the listener is not set
            // with a value or null text behind the icon can be selectable.
            setOnClickListener(null)
        }

    internal val siteSecurityIconView = SiteSecurityIconView(context).apply {
        setPadding(resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_icon_padding))

        // Avoiding text behind the icon being selectable. If the listener is not set
        // with a value or null text behind the icon can be selectable.
        // https://github.com/mozilla-mobile/reference-browser/issues/448
        setOnClickListener(null)
    }

    internal val titleView = AppCompatTextView(context).apply {
        id = R.id.mozac_browser_toolbar_title_view
        gravity = Gravity.CENTER_VERTICAL
        textSize = URL_TEXT_SIZE_SP
        visibility = View.GONE

        setSingleLine(true)
    }

    internal val urlView = AppCompatTextView(context).apply {
        id = R.id.mozac_browser_toolbar_url_view
        gravity = Gravity.CENTER_VERTICAL
        textSize = URL_TEXT_SIZE_SP

        setSingleLine(true)
        isClickable = true
        isFocusable = true

        setOnClickListener {
            if (onUrlClicked()) {
                toolbar.editMode()
            }
        }
    }

    private val menuView = MenuButton(context, toolbar)

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

    init {
        addView(trackingProtectionIconView)
        addView(trackingProtectionAndSecurityIndicatorSeparatorView)
        addView(siteSecurityIconView)
        addView(titleView)
        addView(urlView)
        addView(menuView)
        addView(progressView)
    }

    /**
     * Updates the title to be displayed.
     */
    fun updateTitle(title: String) {
        titleView.text = title
        titleView.isVisible = title.isNotEmpty()
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
        if (!progressView.isVisible && progress > 0) {
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
        browserActions.add(wrapAction(action))
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar.
     */
    fun addPageAction(action: Toolbar.Action) {
        pageActions.add(wrapAction(action))
    }

    /**
     * Adds an action to be displayed on the far left side of the toolbar.
     */
    fun addNavigationAction(action: Toolbar.Action) {
        navigationActions.add(wrapAction(action))
    }

    /**
     * Sets the site's security icon as secure if true, else the regular globe.
     */
    fun setSiteSecurity(secure: SiteSecurity) {
        @ColorInt val color = when (secure) {
            SiteSecurity.INSECURE -> securityIconColor.first
            SiteSecurity.SECURE -> securityIconColor.second
        }
        if (color == Color.TRANSPARENT) {
            siteSecurityIconView.clearColorFilter()
        } else {
            siteSecurityIconView.setColorFilter(color)
        }

        siteSecurityIconView.siteSecurity = secure
        currentSiteSecurity = secure
    }

    /**
     * Sets the site's tracking protection status, as a result the visibility of the tracking
     * protection icon and the separator icon could change:
     * [ON_NO_TRACKERS_BLOCKED] ,[ON_TRACKERS_BLOCKED] and [OFF_FOR_A_SITE] -> icon and separator
     * are visible
     * [OFF_GLOBALLY] -> icon and separator are gone.
     */
    fun setTrackingProtectionState(state: SiteTrackingProtection) {
        if (!displayTrackingProtectionIcon) {
            siteTrackingProtection = state
            return
        }

        val isSeparatorVisible = when (state) {
            ON_NO_TRACKERS_BLOCKED, ON_TRACKERS_BLOCKED, OFF_FOR_A_SITE -> true
            OFF_GLOBALLY -> false
        }

        trackingProtectionAndSecurityIndicatorSeparatorView.apply {
            isVisible = isSeparatorVisible
            setColorFilter(separatorColor)
        }

        trackingProtectionIconView.siteTrackingProtection = state
        siteTrackingProtection = state
    }

    internal fun setTrackingProtectionIcons(
        iconOnNoTrackersBlocked: Drawable,
        iconOnTrackersBlocked: Drawable,
        iconDisabledForSite: Drawable
    ) {

        trackingProtectionIconView.setIcons(
            iconOnNoTrackersBlocked,
            iconOnTrackersBlocked,
            iconDisabledForSite
        )
    }

    /**
     * Declare that the actions (navigation actions, browser actions, page actions) have changed and
     * should be updated if needed.
     */
    fun invalidateActions() {
        menuView.invalidateMenu()
        invalidateActions(navigationActions + pageActions + browserActions)
    }

    /**
     * Set a LongClickListener to the urlView of the toolbar.
     */
    internal fun setOnUrlLongClickListener(handler: ((View) -> Boolean)?) {
        urlView.setOnLongClickListener(handler)
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // This toolbar is using the full size provided by the parent
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        val fixedHeightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        val thirdFixedHeightSpec = MeasureSpec
                .makeMeasureSpec(height / MEASURED_HEIGHT_THIRD_DENOMINATOR, MeasureSpec.EXACTLY)
        setMeasuredDimension(width, height)

        // The security indicator and menu fill the whole height and have a square shape
        val squareSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        measureTrackingProtectionViewsIfNeeded(squareSpec)

        siteSecurityIconView.measure(squareSpec, squareSpec)
        menuView.measure(squareSpec, squareSpec)

        val iconsWidth = siteSecurityIconView.measuredWidth + getTrackingProtectionMeasuredWidth()

        // Measure all actions and use the available height for determining the size (square shape)
        val navigationActionsWidth = measureActions(navigationActions, size = height)
        val browserActionsWidth = measureActions(browserActions, size = height)
        val pageActionsWidth = measureActions(pageActions, size = height)

        // The url uses whatever space is left. Subtract the icon and (optionally) the menu
        val menuWidth = if (menuView.isVisible) height else 0
        val urlWidth = (width - iconsWidth - browserActionsWidth - pageActionsWidth -
            menuWidth - navigationActionsWidth - 2 * urlBoxMargin)
        val urlWidthSpec = MeasureSpec.makeMeasureSpec(urlWidth, MeasureSpec.EXACTLY)

        if (titleView.isVisible) {
            /* With a title view, the url and title split the rest of the space vertically. The
            title view and url should be centered as a singular unit in the middle third. */
            titleView.measure(urlWidthSpec, thirdFixedHeightSpec)
            urlView.measure(urlWidthSpec, thirdFixedHeightSpec)
        } else {
            // With no title view, the url view takes up the rest of the space
            urlView.measure(urlWidthSpec, fixedHeightSpec)
        }

        val progressHeightSpec = MeasureSpec.makeMeasureSpec(
            resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_progress_bar_height),
            MeasureSpec.EXACTLY)
        progressView.measure(widthMeasureSpec, progressHeightSpec)

        urlBoxView?.let {
            val urlBoxWidthSpec = MeasureSpec.makeMeasureSpec(
                iconsWidth + urlWidth + pageActionsWidth - 2 * urlBoxMargin,
                MeasureSpec.EXACTLY
            )
            it.measure(urlBoxWidthSpec, fixedHeightSpec)
        }
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    @Suppress("ComplexMethod")
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

        // The security indicator is always on the far left side of the toolbar. We can lay it out
        // even if it is not going to be displayed. While the tracking protection and its separator
        // are not always visible, we don't lay them out, if they are not visible:
        //   +-------------+-------+-----------------------------------------+
        //   | navigation  | icons |                                         |
        //   |   actions   |       |                                         |
        //   +-------------+-------+-----------------------------------------+

        val (leftSecurityIcon, rightSecurityIcon) = layoutTrackingProtectionViewIfNeeded(
            navigationActionsWidth
        )

        siteSecurityIconView.layout(
            leftSecurityIcon,
            0,
            rightSecurityIcon,
            measuredHeight
        )

        // The menu is always on the far right side of the toolbar:
        //   +-------------+-------+----------------------------------+------+
        //   | navigation  | icons |                                  | menu |
        //   |   actions   |       |                                  |      |
        //   +-------------+-------+----------------------------------+------+

        val menuWidth = if (menuView.isVisible) height else 0
        menuView.layout(measuredWidth - menuView.measuredWidth, 0, measuredWidth, measuredHeight)

        // Now we add browser actions from the left side of the menu to the right (in reversed order):
        //   +-------------+-------+-----------------------+----------+------+
        //   | navigation  | icons |                       | browser  | menu |
        //   |   actions   |       |                       | actions  |      |
        //   +-------------+-------+-----------------------+----------+------+

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
        //   +-------------+--------+-----------------------+----------+------+
        //   | navigation  | icons  |           [ page    ] | browser  | menu |
        //   |   actions   |        |           [ actions ] | actions  |      |
        //   +-------------+--------+-----------------------+----------+------+

        pageActions
            .mapNotNull { it.view }
            .reversed()
            .fold(0) { usedWidth, view ->
                val viewRight = measuredWidth - browserActionWidth - usedWidth - menuWidth - urlBoxMargin
                val viewLeft = viewRight - view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        // Finally the URL uses whatever space is left:
        //   +-------------+--------+-----------------------+----------+------+
        //   | navigation  | icons  | url       [ page    ] | browser  | menu |
        //   |   actions   |        |           [ actions ] | actions  |      |
        //   +-------------+--------+-----------------------+----------+------+
        val iconsWidth = if (siteSecurityIconView.isVisible) {
            siteSecurityIconView.measuredWidth + getTrackingProtectionMeasuredWidth()
        } else 0

        val urlLeft = navigationActionsWidth + iconsWidth + urlBoxMargin

        // If the titleView is visible, it will appear above the URL:
        //   +-------------+-----------+-----------------------+----------+------+
        //   | navigation  | icons  |  title       [ page    ] | browser  | menu |
        //   |   actions   |        |  url         [ actions ] | actions  |      |
        //   +-------------+-----------+-----------------------+----------+------+
        if (titleView.isVisible) {
            val totalTextHeights = urlView.measuredHeight + titleView.measuredHeight
            val totalAvailablePadding = height - totalTextHeights
            val padding = totalAvailablePadding / MEASURED_HEIGHT_DENOMINATOR

            titleView.layout(
                    urlLeft,
                    padding,
                    urlLeft + titleView.measuredWidth,
                    padding + titleView.measuredHeight)
            urlView.layout(
                    urlLeft,
                    padding + titleView.measuredHeight,
                    urlLeft + urlView.measuredWidth,
                    padding + titleView.measuredHeight + urlView.measuredHeight)
        } else {
            urlView.layout(urlLeft, 0, urlLeft + urlView.measuredWidth, measuredHeight)
        }

        // The progress bar by default is going to be drawn at the bottom of the toolbar, top if defined:
        progressView.layout(
            0,
            if (progressBarGravity == TOP_PROGRESS_BAR) 0 else measuredHeight - progressView.measuredHeight,
            measuredWidth,
            if (progressBarGravity == TOP_PROGRESS_BAR) progressView.measuredHeight else measuredHeight
        )

        // The URL box view (if exists) is positioned behind the icon, the url and page actions:

        urlBoxView?.let { view ->
            val urlBoxLeft = navigationActionsWidth + urlBoxMargin
            view.layout(urlBoxLeft, 0, urlBoxLeft + view.measuredWidth, measuredHeight)
        }
    }

    /**
     * Layout the tracking protection views if they are visible and returns where the [siteSecurityIconView]
     * must be layout (left and right) coordinates.
     */
    @VisibleForTesting
    internal fun layoutTrackingProtectionViewIfNeeded(navigationActionsWidth: Int): Pair<Int, Int> {
        val trackingProtectionWidth = trackingProtectionIconView.measuredWidth
        val separatorWidth = trackingProtectionAndSecurityIndicatorSeparatorView.measuredWidth
        val securityWidth = siteSecurityIconView.measuredWidth

        return if (shouldTrackingProtectionViewBeVisible()) {
            trackingProtectionIconView.layout(
                navigationActionsWidth,
                0,
                navigationActionsWidth + trackingProtectionWidth,
                measuredHeight
            )

            trackingProtectionAndSecurityIndicatorSeparatorView.layout(
                trackingProtectionWidth,
                0,
                separatorWidth + trackingProtectionWidth,
                measuredHeight
            )

            separatorWidth + trackingProtectionWidth to
                navigationActionsWidth + separatorWidth + securityWidth + trackingProtectionWidth
        } else {
            navigationActionsWidth to navigationActionsWidth + securityWidth
        }
    }

    private fun getTrackingProtectionMeasuredWidth(): Int {
        return if (trackingProtectionIconView.isVisible) {
            trackingProtectionIconView.measuredWidth + trackingProtectionAndSecurityIndicatorSeparatorView.measuredWidth
        } else 0
    }

    private fun measureTrackingProtectionViewsIfNeeded(squareSpec: Int) {
        if (shouldTrackingProtectionViewBeVisible()) {
            trackingProtectionIconView.measure(squareSpec, squareSpec)
            val height =
                resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_icons_separator_height)
            val width =
                resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_icons_separator_width)

            trackingProtectionAndSecurityIndicatorSeparatorView.measure(width, height)
        }
    }

    private fun shouldTrackingProtectionViewBeVisible() =
        displayTrackingProtectionIcon && (siteTrackingProtection == ON_NO_TRACKERS_BLOCKED ||
            siteTrackingProtection == ON_TRACKERS_BLOCKED)

    companion object {
        internal const val MEASURED_HEIGHT_THIRD_DENOMINATOR = 3
        internal const val MEASURED_HEIGHT_DENOMINATOR = 2

        internal const val BOTTOM_PROGRESS_BAR = 0
        private const val TOP_PROGRESS_BAR = 1
        private const val URL_TEXT_SIZE_SP = 15f
    }
}
