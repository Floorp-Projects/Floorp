/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.drawable.BitmapDrawable
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.support.base.log.Log

/**
 * A browser menu item displaying web extension.
 *
 * @param browserAction Associated [WebExtensionBrowserAction]
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class WebExtensionBrowserMenuItem(
    internal var browserAction: WebExtensionBrowserAction,
    internal val listener: () -> Unit
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_web_extension

    @Suppress("TooGenericExceptionCaught")
    override fun bind(menu: BrowserMenu, view: View) {
        val imageView = view.findViewById<ImageView>(R.id.action_image)
        val labelView = view.findViewById<TextView>(R.id.action_label)
        val badgeView = view.findViewById<TextView>(R.id.badge_text)

        view.isEnabled = browserAction.enabled ?: true

        browserAction.title?.let {
            imageView.contentDescription = it
            labelView.text = it
        }
        browserAction.badgeText?.let { badgeView.text = it }
        browserAction.badgeTextColor?.let { badgeView.setTextColor(it) }
        browserAction.badgeBackgroundColor?.let { badgeView.setBackgroundColor(it) }

        MainScope().launch {
            try {
                val icon = browserAction.loadIcon?.invoke(imageView.measuredHeight)
                icon?.let { imageView.setImageDrawable(BitmapDrawable(view.context.resources, it)) }
            } catch (throwable: Throwable) {
                imageView.setImageResource(R.drawable.mozac_ic_web_extension_default_icon)

                Log.log(
                    Log.Priority.ERROR,
                    "mozac-webextensions",
                    throwable,
                    "Failed to load browser action icon, falling back to default."
                )
            }
        }

        labelView.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }
    }

    override fun invalidate(view: View) {
        val labelView = view.findViewById<TextView>(R.id.action_label)
        val badgeView = view.findViewById<TextView>(R.id.badge_text)

        browserAction.title?.let {
            labelView.text = it
        }

        browserAction.badgeText?.let {
            badgeView.text = it
        }

        labelView.invalidate()
        badgeView.invalidate()
    }
}
