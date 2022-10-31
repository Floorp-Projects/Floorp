/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.view

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.annotation.ColorInt
import mozilla.components.browser.menu2.R
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
class MenuButton2 @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : FrameLayout(context, attrs, defStyleAttr),
    MenuButton,
    View.OnClickListener,
    Observable<MenuButton.Observer> by ObserverRegistry() {

    /**
     * Sets a [MenuController] that will be used to create a menu when this button is clicked.
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

    private val menuIcon: ImageView
    private val highlightView: ImageView
    private val notificationIconView: ImageView

    init {
        View.inflate(context, R.layout.mozac_browser_menu2_button, this)
        setOnClickListener(this)
        menuIcon = findViewById(R.id.icon)
        highlightView = findViewById(R.id.highlight)
        notificationIconView = findViewById(R.id.notification_dot)
    }

    /**
     * Shows the menu.
     */
    override fun onClick(v: View) {
        this.hideKeyboard()
        val menuController = menuController ?: return

        menuController.show(anchor = this)
        notifyObservers { onShow() }
    }

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
    override fun setColorFilter(@ColorInt color: Int) = menuIcon.setColorFilter(color)
}
