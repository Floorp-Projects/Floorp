/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.util.TypedValue
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageButton
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.ktx.android.content.res.pxToDp

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

        for (item in items) {
            val button = AppCompatImageButton(view.context)
            item.updateView(button)

            val outValue = TypedValue()
            view.context.theme.resolveAttribute(
                android.R.attr.selectableItemBackgroundBorderless,
                outValue,
                true
            )

            button.setBackgroundResource(outValue.resourceId)
            button.setOnClickListener {
                item.listener.invoke()
                menu.dismiss()
            }
            // Update the ImageButton when the Button data changes.
            item.register({ item.updateView(button) }, button)

            layout.addView(
                button,
                LinearLayout.LayoutParams(0, view.resources.pxToDp(ICON_HEIGHT_DP), 1f)
            )
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
        @ColorRes val iconTintColorResource: Int = 0,
        val isEnabled: () -> Boolean = { true },
        delegate: Observable<() -> Unit> = ObserverRegistry(),
        val listener: () -> Unit
    ) : Observable<() -> Unit> by delegate {

        /**
         * Re-draws the button in the toolbar. Call to notify the toolbar when the button state
         * changes and should be enabled or disabled.
         */
        fun update() {
            notifyObservers { invoke() }
        }

        internal open fun updateView(view: ImageView) {
            view.setImageResource(imageResource)
            view.contentDescription = contentDescription
            if (iconTintColorResource != 0) {
                view.imageTintList =
                    ContextCompat.getColorStateList(view.context, iconTintColorResource)
            }
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
        @ColorRes val primaryImageTintResource: Int = 0,
        @DrawableRes val secondaryImageResource: Int = primaryImageResource,
        val secondaryContentDescription: String = primaryContentDescription,
        @ColorRes val secondaryImageTintResource: Int = primaryImageTintResource,
        val isInPrimaryState: () -> Boolean = { true },
        val disableInSecondaryState: Boolean = false,
        delegate: Observable<() -> Unit> = ObserverRegistry(),
        listener: () -> Unit
    ) : Button(
        primaryImageResource,
        primaryContentDescription,
        primaryImageTintResource,
        isInPrimaryState,
        delegate,
        listener = listener
    ) {

        override fun updateView(view: ImageView) {
            if (isInPrimaryState()) {
                super.updateView(view)
            } else {
                view.setImageResource(secondaryImageResource)
                view.contentDescription = secondaryContentDescription
                if (secondaryImageTintResource != 0) {
                    view.imageTintList =
                        ContextCompat.getColorStateList(view.context, secondaryImageTintResource)
                }
                view.isEnabled = !disableInSecondaryState
            }
        }
    }

    companion object {
        private const val ICON_HEIGHT_DP = 24
    }
}
