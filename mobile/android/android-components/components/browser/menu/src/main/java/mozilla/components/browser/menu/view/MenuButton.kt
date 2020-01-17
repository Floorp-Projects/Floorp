/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenu.Orientation
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.R

/**
 * A `three-dot` button used for expanding menus.
 *
 * If you are using a browser toolbar, do not use this class directly.
 */
class MenuButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), View.OnClickListener {

    /**
     * Listener called when the menu is shown.
     */
    var onShow: () -> Unit = {}
    /**
     * Listener called when the menu is dismissed.
     */
    var onDismiss: () -> Unit = {}
    /**
     * Callback to get the orientation for the menu.
     * This is called every time the menu should be displayed.
     */
    var getOrientation: () -> Orientation = {
        BrowserMenu.determineMenuOrientation(parent as? View?)
    }

    var menuBuilder: BrowserMenuBuilder? = null
        set(value) {
            field = value
            menu?.dismiss()
            if (value == null) menu = null
        }

    @VisibleForTesting internal var menu: BrowserMenu? = null

    private val menuIcon: ImageView
    private val highlightView: ImageView
    private val notificationIconView: ImageView

    init {
        View.inflate(context, R.layout.mozac_browser_menu_button, this)
        setOnClickListener(this)
        menuIcon = findViewById(R.id.icon)
        highlightView = findViewById(R.id.highlight)
        notificationIconView = findViewById(R.id.notification_dot)
    }

    /**
     * Shows the menu, or dismisses it if already open.
     */
    override fun onClick(v: View) {
        if (menu != null) {
            menu?.dismiss()
            return
        }

        menu = menuBuilder?.build(context)
        val endAlwaysVisible = menuBuilder?.endOfMenuAlwaysVisible ?: false
        menu?.show(
            anchor = this,
            orientation = getOrientation(),
            endOfMenuAlwaysVisible = endAlwaysVisible
        ) {
            menu = null
            onDismiss()
        }
        onShow()
    }

    /**
     * Show the indicator for a browser menu highlight.
     */
    @Suppress("Deprecation")
    fun setHighlight(highlight: BrowserMenuHighlight?) {
        when (highlight) {
            is BrowserMenuHighlight.HighPriority -> {
                highlightView.imageTintList = ColorStateList.valueOf(highlight.backgroundTint)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            is BrowserMenuHighlight.LowPriority -> {
                notificationIconView.setColorFilter(highlight.notificationTint)
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.VISIBLE
            }
            is BrowserMenuHighlight.ClassicHighlight -> {
                val color = ContextCompat.getColor(context, highlight.colorResource)
                highlightView.imageTintList = ColorStateList.valueOf(color)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            null -> {
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.GONE
            }
        }
    }

    /**
     * Sets the tint of the 3-dot menu icon.
     */
    fun setColorFilter(@ColorInt color: Int) {
        menuIcon.setColorFilter(color)
    }

    /**
     * Dismiss the menu, if open.
     */
    fun dismissMenu() {
        menu?.dismiss()
    }

    /**
     * Invalidates the [BrowserMenu], if open.
     */
    fun invalidateBrowserMenu() {
        menu?.invalidate()
    }
}
