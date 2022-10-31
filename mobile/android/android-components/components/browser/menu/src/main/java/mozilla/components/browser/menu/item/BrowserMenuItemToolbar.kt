/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.os.Build
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.widget.ImageView
import android.widget.LinearLayout
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageButton
import androidx.appcompat.widget.TooltipCompat
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.SmallMenuCandidate
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * A toolbar of buttons to show inside the browser menu.
 *
 * @param items buttons that will be shown in a horizontal layout
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 */
class BrowserMenuItemToolbar(
    private val items: List<Button>,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override val interactiveCount: () -> Int = { items.size }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_toolbar

    override fun bind(menu: BrowserMenu, view: View) {
        val layout = view as LinearLayout
        layout.removeAllViews()

        val selectableBackground =
            layout.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)

        for (item in items) {
            val button = AppCompatImageButton(layout.context)
            item.bind(button)

            button.setFocusable(true)
            button.setBackgroundResource(selectableBackground)
            button.setOnClickListener {
                item.listener()
                menu.dismiss()
            }
            button.setOnLongClickListener {
                item.longClickListener?.invoke()
                menu.dismiss()
                true
            }
            button.isLongClickable = item.longClickListener != null

            layout.addView(button, LinearLayout.LayoutParams(0, MATCH_PARENT, 1f))
        }
    }

    override fun invalidate(view: View) {
        val layout = view as LinearLayout
        items.withIndex().forEach { (index, item) ->
            item.invalidate(layout.getChildAt(index) as AppCompatImageButton)
        }
    }

    override fun asCandidate(context: Context) = RowMenuCandidate(
        items = items.map { it.asCandidate(context) },
        containerStyle = ContainerStyle(isVisible = visible()),
    )

    /**
     * A button to be shown in a toolbar inside the browser menu.
     *
     * @param imageResource ID of a drawable resource to be shown as icon.
     * @param contentDescription The button's content description, used for accessibility support.
     * @param iconTintColorResource Optional ID of color resource to tint the icon.
     * @param isEnabled Lambda to return true/false to indicate if this button should be enabled or disabled.
     * @param longClickListener Callback to be invoked when the button is long clicked.
     * @param listener Callback to be invoked when the button is pressed.
     */
    @Suppress("LongParameterList")
    open class Button(
        @DrawableRes val imageResource: Int,
        val contentDescription: String,
        @ColorRes val iconTintColorResource: Int = NO_ID,
        val isEnabled: () -> Boolean = { true },
        val longClickListener: (() -> Unit)? = null,
        val listener: () -> Unit,
    ) {

        internal open fun bind(view: ImageView) {
            view.setImageResource(imageResource)
            view.contentDescription = contentDescription
            setTooltipTextCompatible(view, contentDescription)
            view.setTintResource(iconTintColorResource)
            view.isEnabled = isEnabled()
        }

        internal open fun invalidate(view: ImageView) {
            view.isEnabled = isEnabled()
        }

        internal open fun asCandidate(context: Context) = SmallMenuCandidate(
            contentDescription,
            icon = DrawableMenuIcon(
                context,
                resource = imageResource,
                tint = if (iconTintColorResource == NO_ID) null else getColor(context, iconTintColorResource),
            ),
            containerStyle = ContainerStyle(isEnabled = isEnabled()),
            onClick = listener,
        )

        internal fun setTooltipTextCompatible(view: ImageView, contentDescription: String) {
            if (Build.VERSION.SDK_INT == Build.VERSION_CODES.O) {
                CustomTooltip.setTooltipText(view, contentDescription)
            } else {
                TooltipCompat.setTooltipText(view, contentDescription)
            }
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
     * @param longClickListener Callback to be invoked when the button is long clicked.
     * @param listener Callback to be invoked when the button is pressed.
     */
    @Suppress("LongParameterList")
    open class TwoStateButton(
        @DrawableRes val primaryImageResource: Int,
        val primaryContentDescription: String,
        @ColorRes val primaryImageTintResource: Int = NO_ID,
        @DrawableRes val secondaryImageResource: Int = primaryImageResource,
        val secondaryContentDescription: String = primaryContentDescription,
        @ColorRes val secondaryImageTintResource: Int = primaryImageTintResource,
        val isInPrimaryState: () -> Boolean = { true },
        val disableInSecondaryState: Boolean = false,
        longClickListener: (() -> Unit)? = null,
        listener: () -> Unit,
    ) : Button(
        primaryImageResource,
        primaryContentDescription,
        primaryImageTintResource,
        isInPrimaryState,
        longClickListener = longClickListener,
        listener = listener,
    ) {

        private var wasInPrimaryState = false

        override fun bind(view: ImageView) {
            if (isInPrimaryState()) {
                super.bind(view)
            } else {
                view.setImageResource(secondaryImageResource)
                view.contentDescription = secondaryContentDescription
                setTooltipTextCompatible(view, secondaryContentDescription)
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

        override fun asCandidate(context: Context): SmallMenuCandidate = if (isInPrimaryState()) {
            super.asCandidate(context)
        } else {
            SmallMenuCandidate(
                secondaryContentDescription,
                icon = DrawableMenuIcon(
                    context,
                    resource = secondaryImageResource,
                    tint = if (secondaryImageTintResource == NO_ID) {
                        null
                    } else {
                        getColor(context, secondaryImageTintResource)
                    },
                ),
                containerStyle = ContainerStyle(isEnabled = !disableInSecondaryState),
                onClick = listener,
            )
        }
    }
}
