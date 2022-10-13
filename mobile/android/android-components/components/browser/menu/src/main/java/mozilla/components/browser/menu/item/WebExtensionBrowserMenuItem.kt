/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.graphics.drawable.Drawable
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.drawable.toDrawable
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.base.log.Log

/**
 * A browser menu item displaying a web extension action.
 *
 * @param action the [Action] to display.
 * @param listener a callback to be invoked when this menu item is clicked.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 */
class WebExtensionBrowserMenuItem(
    internal var action: Action,
    internal val listener: () -> Unit,
    internal val id: String = "",
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_web_extension

    @VisibleForTesting
    internal var iconTintColorResource: Int? = null

    @Suppress("TooGenericExceptionCaught")
    override fun bind(menu: BrowserMenu, view: View) {
        val imageView = view.findViewById<ImageView>(R.id.action_image)
        val labelView = view.findViewById<TextView>(R.id.action_label)
        val badgeView = view.findViewById<TextView>(R.id.badge_text)
        val container = view.findViewById<View>(R.id.container)

        container.isEnabled = action.enabled ?: true

        action.title?.let {
            imageView.contentDescription = it
            labelView.text = it
        }
        badgeView.setBadgeText(action.badgeText)
        action.badgeTextColor?.let { badgeView.setTextColor(it) }
        action.badgeBackgroundColor?.let { badgeView.background?.setTint(it) }

        setupIcon(view, imageView, iconTintColorResource)

        container.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }
    }

    override fun invalidate(view: View) {
        val labelView = view.findViewById<TextView>(R.id.action_label)
        val badgeView = view.findViewById<TextView>(R.id.badge_text)

        action.title?.let {
            labelView.text = it
        }
        badgeView.setBadgeText(action.badgeText)

        labelView.invalidate()
        badgeView.invalidate()
    }

    override fun asCandidate(context: Context) = TextMenuCandidate(
        action.title.orEmpty(),
        start = AsyncDrawableMenuIcon(
            loadDrawable = { _, height -> loadIcon(context, height) },
        ),
        end = action.badgeText?.let { badgeText ->
            TextMenuIcon(
                badgeText,
                backgroundTint = action.badgeBackgroundColor,
                textStyle = TextStyle(
                    color = action.badgeTextColor,
                ),
            )
        },
        containerStyle = ContainerStyle(
            isVisible = visible(),
            isEnabled = action.enabled ?: false,
        ),
        onClick = listener,
    )

    @VisibleForTesting
    internal fun setupIcon(view: View, imageView: ImageView, iconTintColorResource: Int?) {
        MainScope().launch {
            loadIcon(view.context, imageView.measuredHeight)?.let {
                iconTintColorResource?.let { tint -> imageView.setTintResource(tint) }
                imageView.setImageDrawable(it)
            }
        }
    }

    @Suppress("TooGenericExceptionCaught")
    private suspend fun loadIcon(context: Context, height: Int): Drawable? {
        return try {
            action.loadIcon?.invoke(height)?.toDrawable(context.resources)
        } catch (throwable: Throwable) {
            Log.log(
                Log.Priority.ERROR,
                "mozac-webextensions",
                throwable,
                "Failed to load browser action icon, falling back to default.",
            )

            getDrawable(context, R.drawable.mozac_ic_web_extension_default_icon)
        }
    }

    /**
     * Sets the tint to be applied to the extension icon
     */
    fun setIconTint(iconTintColorResource: Int?) {
        iconTintColorResource?.let { this.iconTintColorResource = it }
    }
}

/**
 * Sets the badgeText and the visibility of the TextView based on empty/nullability of the badgeText.
 */
fun TextView.setBadgeText(badgeText: String?) {
    if (badgeText.isNullOrEmpty()) {
        visibility = View.INVISIBLE
    } else {
        visibility = View.VISIBLE
        text = badgeText
    }
}
