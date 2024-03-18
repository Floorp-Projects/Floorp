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
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenu.Orientation
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.MenuButton
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.MenuEffect
import mozilla.components.concept.menu.ext.effects
import mozilla.components.concept.menu.ext.max
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * A `three-dot` button used for expanding menus.
 *
 * If you are using a browser toolbar, do not use this class directly.
 */
class MenuButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : FrameLayout(context, attrs, defStyleAttr),
    MenuButton,
    View.OnClickListener,
    Observable<MenuButton.Observer> by ObserverRegistry() {

    private val menuControllerObserver = object : MenuController.Observer {
        /**
         * Change the menu button appearance when the menu list changes.
         */
        override fun onMenuListSubmit(list: List<MenuCandidate>) {
            val effect = list.effects().max()

            // If a highlighted item is found, show the indicator
            setEffect(effect)
        }

        override fun onDismiss() = notifyObservers { onDismiss() }
    }

    /**
     * Listener called when the menu is shown.
     */
    @Deprecated("Use the Observable interface to listen for onShow")
    var onShow: () -> Unit = {}

    /**
     * Listener called when the menu is dismissed.
     */
    @Deprecated("Use the Observable interface to listen for onDismiss")
    var onDismiss: () -> Unit = {}

    /**
     * Callback to get the orientation for the menu.
     * This is called every time the menu should be displayed.
     * This has no effect when a [MenuController] is set.
     */
    var getOrientation: () -> Orientation = {
        BrowserMenu.determineMenuOrientation(parent as? View?)
    }

    /**
     * Sets a [MenuController] that will be used to create a menu when this button is clicked.
     * If present, [menuBuilder] will be ignored.
     */
    override var menuController: MenuController? = null
        set(value) {
            // Clean up old controller
            field?.dismiss()
            field?.unregister(menuControllerObserver)

            // Attach new controller
            field = value
            value?.register(menuControllerObserver, this)
        }

    /**
     * Sets a [BrowserMenuBuilder] that will be used to create a menu when this button is clicked.
     */
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

        // Hook up deprecated callbacks using new observer system
        @Suppress("Deprecation")
        val internalObserver = object : MenuButton.Observer {
            override fun onShow() = this@MenuButton.onShow()
            override fun onDismiss() = this@MenuButton.onDismiss()
        }
        register(internalObserver)
    }

    /**
     * Shows the menu, or dismisses it if already open.
     */
    override fun onClick(v: View) {
        this.hideKeyboard()

        // If a legacy menu is open, dismiss it.
        if (menu != null) {
            menu?.dismiss()
            return
        }

        val menuController = menuController
        if (menuController != null) {
            // Use the newer menu controller if set
            menuController.show(anchor = this)
        } else {
            menu = menuBuilder?.build(context)
            val endAlwaysVisible = menuBuilder?.endOfMenuAlwaysVisible ?: false
            menu?.show(
                anchor = this,
                orientation = getOrientation(),
                endOfMenuAlwaysVisible = endAlwaysVisible,
            ) {
                menu = null
                notifyObservers { onDismiss() }
            }
        }
        notifyObservers { onShow() }
    }

    /**
     * Show the indicator for a browser menu highlight.
     */
    fun setHighlight(highlight: BrowserMenuHighlight?) =
        setEffect(highlight?.asEffect(context))

    /**
     * Show the indicator for a browser menu effect.
     */
    override fun setEffect(effect: MenuEffect?) {
        when (effect) {
            is HighPriorityHighlightEffect -> {
                highlightView.imageTintList = ColorStateList.valueOf(effect.backgroundTint)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            is LowPriorityHighlightEffect -> {
                notificationIconView.setColorFilter(effect.notificationTint)
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.VISIBLE
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
    override fun setColorFilter(@ColorInt color: Int) {
        menuIcon.setColorFilter(color)
    }

    /**
     * Dismiss the menu, if open.
     */
    fun dismissMenu() {
        menuController?.dismiss()
        menu?.dismiss()
    }

    /**
     * Invalidates the [BrowserMenu], if open.
     */
    fun invalidateBrowserMenu() {
        menu?.invalidate()
    }
}
