/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.ktx.android.view.dp
import mozilla.components.support.ktx.android.view.isVisible
import mozilla.components.ui.progress.AnimatedProgressBar

/**
 * Sub-component of the browser toolbar responsible for displaying the URL and related controls.
 *
 * Structure:
 *   +-------------+------+-----------------------+----------+------+
 *   | navigation  | icon | url       [ page    ] | browser  | menu |
 *   |   actions   |      |           [ actions ] | actions  |      |
 *   +-------------+------+----------------------------------+------+
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
internal class DisplayToolbar(
    context: Context,
    val toolbar: BrowserToolbar
) : ViewGroup(context) {
    internal var menuBuilder: BrowserMenuBuilder? = null
        set(value) {
            field = value
            menuView.visibility = if (value == null)
                View.GONE
            else
                View.VISIBLE
        }

    internal val iconView = ImageView(context).apply {
        val padding = dp(ICON_PADDING_DP)
        setPadding(padding, padding, padding, padding)

        setImageResource(mozilla.components.ui.icons.R.drawable.mozac_ic_globe)
    }

    internal val urlView = TextView(context).apply {
        gravity = Gravity.CENTER_VERTICAL
        textSize = URL_TEXT_SIZE
        setFadingEdgeLength(URL_FADING_EDGE_SIZE_DP)
        isHorizontalFadingEdgeEnabled = true

        setSingleLine(true)

        setOnClickListener {
            toolbar.editMode()
        }
    }

    private val menuView = ImageButton(context).apply {
        val padding = dp(MENU_PADDING_DP)
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
            menuBuilder?.build(context)?.show(this)
        }
    }

    private val progressView = AnimatedProgressBar(context)

    private val browserActions: MutableList<DisplayAction> = mutableListOf()
    private val pageActions: MutableList<DisplayAction> = mutableListOf()
    private val navigationActions: MutableList<DisplayAction> = mutableListOf()

    private var urlBackgroundRect = Rect()

    // Background to be drawn behind the URL (including page actions)
    internal var urlBackgroundDrawable: Drawable? = null

    init {
        addView(iconView)
        addView(urlView)
        addView(menuView)
        addView(progressView)

        setWillNotDraw(false)
    }

    /**
     * Updates the URL to be displayed.
     */
    fun updateUrl(url: String) {
        urlView.text = url
    }

    /**
     * Updates the progress to be displayed.
     */
    fun updateProgress(progress: Int) {
        progressView.progress = progress

        progressView.visibility = if (progress < progressView.max) View.VISIBLE else View.GONE
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar.
     *
     * If there is not enough room to show all icons then some icons may be moved to an overflow
     * menu.
     */
    fun addBrowserAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        browserActions.add(displayAction)

        if (browserActions.size <= MAX_VISIBLE_ACTION_ITEMS) {
            val view = createActionView(context, action)

            displayAction.view = view

            addView(view)
        }
    }

    /**
     * Adds an action to be displayed on the right side of the toolbar.
     */
    fun addPageAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        createActionView(context, action).let {
            displayAction.view = it
            addView(it)
        }

        pageActions.add(displayAction)
    }

    /**
     * Adds an action to be displayed on the far left side of the toolbar.
     */
    fun addNavigationAction(action: Toolbar.Action) {
        val displayAction = DisplayAction(action)

        createActionView(context, action).let {
            displayAction.view = it
            addView(it)
        }

        navigationActions.add(displayAction)
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // This toolbar is using the full size provided by the parent
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        setMeasuredDimension(width, height)

        // The icon and menu fill the whole height and have a square shape
        val squareSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        iconView.measure(squareSpec, squareSpec)
        menuView.measure(squareSpec, squareSpec)

        // Measure all actions and use the available height for determining the size (square shape)
        val navigationActionsWidth = measureActions(navigationActions, size = height)
        val browserActionsWidth = measureActions(browserActions, size = height)
        val pageActionsWidth = measureActions(pageActions, size = height)

        // The url uses whatever space is left. Substract the icon and (optionally) the menu
        val menuWidth = if (menuView.isVisible()) height else 0
        val urlWidth = width - height - browserActionsWidth - pageActionsWidth - menuWidth
        val urlWidthSpec = MeasureSpec.makeMeasureSpec(urlWidth, MeasureSpec.EXACTLY)
        urlView.measure(urlWidthSpec, heightMeasureSpec)

        val progressHeightSpec = MeasureSpec.makeMeasureSpec(dp(PROGRESS_BAR_HEIGHT_DP), MeasureSpec.EXACTLY)
        progressView.measure(widthMeasureSpec, progressHeightSpec)
    }

    /**
     * Measures a list of actions and returns the needed width.
     */
    private fun measureActions(actions: List<DisplayAction>, size: Int): Int {
        val sizeSpec = MeasureSpec.makeMeasureSpec(size, MeasureSpec.EXACTLY)

        return actions
            .mapNotNull { it.view }
            .map { view ->
                view.measure(sizeSpec, sizeSpec)
                size
            }
            .sum()
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        // First we layout the navigation actions if there are any
        val navigationActionsWidth = navigationActions
            .mapNotNull { it.view }
            .fold(0) { usedWidth, view ->
                val viewLeft = usedWidth
                val viewRight = viewLeft + view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        // The icon is always on the left side of the toolbar
        iconView.layout(navigationActionsWidth, 0, navigationActionsWidth + iconView.measuredWidth, measuredHeight)

        // The menu is always on the right side of the toolbar
        val menuWidth = if (menuView.isVisible()) height else 0
        menuView.layout(measuredWidth - menuView.measuredWidth, 0, measuredWidth, measuredHeight)

        // Now we add browser actions from the left side of the menu to the right (in reversed order)
        val browserActionWidth = browserActions
            .mapNotNull { it.view }
            .reversed()
            .fold(0) { usedWidth, view ->
                val viewRight = measuredWidth - usedWidth - menuWidth
                val viewLeft = viewRight - view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        // After browser actions we add page actions from the right to the left (in reversed order)
        val pageActionsWidth = pageActions
            .mapNotNull { it.view }
            .reversed()
            .fold(0) { usedWidth, view ->
                val viewRight = measuredWidth - browserActionWidth - usedWidth - menuWidth
                val viewLeft = viewRight - view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }

        val iconWidth = if (iconView.isVisible()) iconView.measuredWidth else 0
        val urlRight = measuredWidth - browserActionWidth - pageActionsWidth - menuWidth
        val urlLeft = navigationActionsWidth + iconWidth
        urlView.layout(urlLeft, 0, urlRight, measuredHeight)

        progressView.layout(0, measuredHeight - progressView.measuredHeight, measuredWidth, measuredHeight)

        urlBackgroundRect.set(navigationActionsWidth, 0, urlRight + pageActionsWidth, measuredHeight)
    }

    override fun onDraw(canvas: Canvas) {
        urlBackgroundDrawable?.let { drawable ->
            canvas.save()

            canvas.translate(urlBackgroundRect.left.toFloat(), urlBackgroundRect.top.toFloat())
            drawable.setBounds(0, 0, urlBackgroundRect.width(), urlBackgroundRect.height())
            drawable.draw(canvas)

            canvas.restore()
        }
    }

    companion object {
        private const val ICON_PADDING_DP = 16
        private const val MENU_PADDING_DP = 16
        private const val ACTION_PADDING_DP = 16
        private const val URL_TEXT_SIZE = 15f
        private const val URL_FADING_EDGE_SIZE_DP = 24
        private const val PROGRESS_BAR_HEIGHT_DP = 3
        private const val MAX_VISIBLE_ACTION_ITEMS = 2

        fun createActionView(context: Context, action: Toolbar.Action) = ImageButton(context).apply {
            val padding = dp(ACTION_PADDING_DP)
            setPadding(padding, padding, padding, padding)

            setImageResource(action.imageResource)
            contentDescription = action.contentDescription

            val outValue = TypedValue()
            context.theme.resolveAttribute(
                    android.R.attr.selectableItemBackgroundBorderless,
                    outValue,
                    true)

            setBackgroundResource(outValue.resourceId)

            setOnClickListener { action.listener.invoke() }
        }
    }
}

/**
 * A wrapper helper to pair a Toolbar.Action with an optional View.
 */
private class DisplayAction(
    var actual: Toolbar.Action,
    var view: View? = null
)
