/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageButton
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * A toolbar of buttons to show inside the browser menu.
 */
class BrowserMenuItemToolbar(
    private val items: List<Button>
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_toolbar

    override fun bind(menu: BrowserMenu, view: View) {
        val layout = view as LinearLayout
        layout.removeAllViews()

        val selectableBackground =
            layout.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)
        val iconHeight = view.resources.getDimensionPixelSize(R.dimen.mozac_browser_menu_item_image_text_icon_height)

        for (item in items) {
            val button = AppCompatImageButton(layout.context)
            item.bind(button)

            button.setBackgroundResource(selectableBackground)
            button.setOnClickListener {
                item.listener.invoke()
                menu.dismiss()
            }

            layout.addView(
                button,
                LinearLayout.LayoutParams(0, iconHeight, 1f)
            )
        }
    }

    override fun invalidate(view: View) {
        val layout = view as LinearLayout
        items.withIndex().forEach {
            val (index, item) = it
            item.invalidate(layout.getChildAt(index) as AppCompatImageButton)
        }
    }

    /**
     * A button to be shown in a toolbar inside the browser menu.
     *
     * @param imageResource ID of a drawable resource to be shown as icon.
     * @param contentDescription The button's content description, used for accessibility support.
     * @param iconTintColorResource Optional ID of color resource to tint the icon.
     * @param isEnabled Lambda to return true/false to indicate if this button should be enabled or disabled.
     * @param listener Callback to be invoked when the button is pressed.
     */
    open class Button(
        @DrawableRes val imageResource: Int,
        val contentDescription: String,
        @ColorRes val iconTintColorResource: Int = NO_ID,
        val isEnabled: () -> Boolean = { true },
        val listener: () -> Unit
    ) {

        internal open fun bind(view: ImageView) {
            view.setImageResource(imageResource)
            view.contentDescription = contentDescription
            view.setTintResource(iconTintColorResource)
            view.isEnabled = isEnabled()
        }

        internal open fun invalidate(view: ImageView) {
            view.isEnabled = isEnabled()
        }
    }

    /**
     * A button that either shows an primary state or an secondary state based on the provided
     * <code>isInPrimaryState</code> lambda.
     *
     * @param primaryImageResource ID of a drawable resource to be shown as primary icon.
     * @param primaryContentDescription The button's primary content description, used for accessibility support.
     * @param primaryImageTintResource Optional ID of color resource to tint the primary icon.
     * @param secondaryImageResource Optional ID of a different drawable resource to be shown as secondary icon.
     * @param secondaryContentDescription Optional secondary content description for button, for accessibility support.
     * @param secondaryImageTintResource Optional ID of secondary color resource to tint the icon.
     * @param isInPrimaryState Lambda to return true/false to indicate if this button should be primary or secondary.
     * @param disableInSecondaryState Optional boolean to disable the button when in secondary state.
     * @param listener Callback to be invoked when the button is pressed.
     */
    open class TwoStateButton(
        @DrawableRes val primaryImageResource: Int,
        val primaryContentDescription: String,
        @ColorRes val primaryImageTintResource: Int = NO_ID,
        @DrawableRes val secondaryImageResource: Int = primaryImageResource,
        val secondaryContentDescription: String = primaryContentDescription,
        @ColorRes val secondaryImageTintResource: Int = primaryImageTintResource,
        val isInPrimaryState: () -> Boolean = { true },
        val disableInSecondaryState: Boolean = false,
        listener: () -> Unit
    ) : Button(
        primaryImageResource,
        primaryContentDescription,
        primaryImageTintResource,
        isInPrimaryState,
        listener = listener
    ) {

        private var wasInPrimaryState = false

        override fun bind(view: ImageView) {
            if (isInPrimaryState()) {
                super.bind(view)
            } else {
                view.setImageResource(secondaryImageResource)
                view.contentDescription = secondaryContentDescription
                view.setTintResource(secondaryImageTintResource)
                view.isEnabled = !disableInSecondaryState
            }
            wasInPrimaryState = isInPrimaryState()
        }

        override fun invalidate(view: ImageView) {
            if (isInPrimaryState() != wasInPrimaryState) {
                bind(view)
            }
        }
    }

    companion object {
        private const val ICON_HEIGHT_DP = 24
    }
}
