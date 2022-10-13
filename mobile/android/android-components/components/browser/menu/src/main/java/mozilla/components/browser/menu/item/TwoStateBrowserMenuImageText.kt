/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

/**
 * A browser menu item with two states, used for displaying text with an image icon
 *
 * @param primaryLabel The visible label of the checkbox in primary state.
 * @param secondaryLabel The visible label of this menu item in secondary state.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @param primaryStateIconResource ID of a drawable resource to be shown as icon in primary state.
 * @param secondaryStateIconResource ID of a drawable resource to be shown as icon in secondary state.
 * @param iconTintColorResource Optional ID of color resource to tint the checkbox drawable.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param isInPrimaryState Lambda to return true/false to indicate item is in primary state.
 * @param isInSecondaryState Lambda to return true/false to indicate item is in secondary state
 * @param primaryStateAction Callback to be invoked when this menu item is clicked in primary state.
 * @param secondaryStateAction Callback to be invoked when this menu item is clicked in secondary state.
 */
@Suppress("LongParameterList")
class TwoStateBrowserMenuImageText(
    private val primaryLabel: String,
    private val secondaryLabel: String,
    @ColorRes internal val textColorResource: Int = NO_ID,
    @DrawableRes val primaryStateIconResource: Int,
    @DrawableRes val secondaryStateIconResource: Int,
    @ColorRes iconTintColorResource: Int = NO_ID,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    val isInPrimaryState: () -> Boolean = { true },
    val isInSecondaryState: () -> Boolean = { false },
    private val primaryStateAction: () -> Unit = { },
    private val secondaryStateAction: () -> Unit = { },
) : BrowserMenuImageText(
    primaryLabel,
    primaryStateIconResource,
    iconTintColorResource,
    textColorResource,
    isCollapsingMenuLimit,
    isSticky,
    primaryStateAction,
) {
    override var visible: () -> Boolean = { isInPrimaryState() || isInSecondaryState() }

    override fun getLayoutResource(): Int =
        R.layout.mozac_browser_menu_item_image_text

    override fun bind(menu: BrowserMenu, view: View) {
        val isInPrimaryState = isInPrimaryState()
        bindText(view, isInPrimaryState)
        bindImage(view, isInPrimaryState)

        val listener = if (isInPrimaryState) primaryStateAction else secondaryStateAction
        view.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }
    }

    private fun bindText(view: View, isInPrimaryState: Boolean) {
        val textView = view.findViewById<TextView>(R.id.text)
        textView.text = if (isInPrimaryState) primaryLabel else secondaryLabel
        textView.setColorResource(textColorResource)
    }

    private fun bindImage(view: View, isInPrimaryState: Boolean) {
        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        val imageResource =
            if (isInPrimaryState) primaryStateIconResource else secondaryStateIconResource

        with(imageView) {
            setImageResource(imageResource)
            setTintResource(iconTintColorResource)
        }
    }

    override fun asCandidate(context: Context): MenuCandidate = TextMenuCandidate(
        if (isInPrimaryState()) {
            primaryLabel
        } else {
            secondaryLabel
        },
        start = DrawableMenuIcon(
            context,
            resource = if (isInPrimaryState()) {
                primaryStateIconResource
            } else {
                secondaryStateIconResource
            },
            tint = if (iconTintColorResource == NO_ID) {
                null
            } else {
                ContextCompat.getColor(
                    context,
                    iconTintColorResource,
                )
            },
        ),
        textStyle = TextStyle(
            color = if (textColorResource == NO_ID) {
                null
            } else {
                ContextCompat.getColor(
                    context,
                    textColorResource,
                )
            },
        ),
        containerStyle = ContainerStyle(isVisible = visible()),
        onClick = if (isInPrimaryState()) primaryStateAction else secondaryStateAction,
    )
}
