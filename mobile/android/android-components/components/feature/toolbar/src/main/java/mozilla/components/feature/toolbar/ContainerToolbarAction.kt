/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.content.Context
import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import androidx.annotation.ColorInt
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.state.ContainerState.Color
import mozilla.components.browser.state.state.ContainerState.Icon
import mozilla.components.concept.toolbar.Toolbar.Action
import mozilla.components.support.base.android.Padding
import mozilla.components.support.ktx.android.view.setPadding
import mozilla.components.support.utils.DrawableUtils.loadAndTintDrawable

/**
 * An action button that represents a container to be added to the toolbar.
 *
 * @param container Associated [ContainerState]'s icon and color to render in the toolbar.
 * @param padding A optional custom padding.
 * @param listener A optional callback that will be invoked whenever the button is pressed.
 */
class ContainerToolbarAction(
    internal val container: ContainerState,
    internal val padding: Padding? = null,
    private var listener: (() -> Unit)? = null,
) : Action {
    override fun createView(parent: ViewGroup): View {
        val rootView = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_feature_toolbar_container_action_layout, parent, false)

        listener?.let { clickListener ->
            rootView.setOnClickListener { clickListener.invoke() }
        }

        padding?.let { rootView.setPadding(it) }

        return rootView
    }

    override fun bind(view: View) {
        val imageView = view.findViewById<ImageView>(R.id.container_action_image)
        imageView.contentDescription = container.name
        imageView.setImageDrawable(getIcon(view.context, container))
    }

    @Suppress("ComplexMethod")
    internal fun getIcon(context: Context, container: ContainerState): Drawable {
        @ColorInt val tint = getTint(context, container.color)

        return when (container.icon) {
            Icon.FINGERPRINT -> loadAndTintDrawable(context, R.drawable.mozac_ic_fingerprint, tint)
            Icon.BRIEFCASE -> loadAndTintDrawable(context, R.drawable.mozac_ic_briefcase, tint)
            Icon.DOLLAR -> loadAndTintDrawable(context, R.drawable.mozac_ic_dollar, tint)
            Icon.CART -> loadAndTintDrawable(context, R.drawable.mozac_ic_cart, tint)
            Icon.CIRCLE -> loadAndTintDrawable(context, R.drawable.mozac_ic_circle, tint)
            Icon.GIFT -> loadAndTintDrawable(context, R.drawable.mozac_ic_gift, tint)
            Icon.VACATION -> loadAndTintDrawable(context, R.drawable.mozac_ic_vacation, tint)
            Icon.FOOD -> loadAndTintDrawable(context, R.drawable.mozac_ic_food, tint)
            Icon.FRUIT -> loadAndTintDrawable(context, R.drawable.mozac_ic_fruit, tint)
            Icon.PET -> loadAndTintDrawable(context, R.drawable.mozac_ic_pet, tint)
            Icon.TREE -> loadAndTintDrawable(context, R.drawable.mozac_ic_tree, tint)
            Icon.CHILL -> loadAndTintDrawable(context, R.drawable.mozac_ic_chill, tint)
            Icon.FENCE -> loadAndTintDrawable(context, R.drawable.mozac_ic_fence, tint)
        }
    }

    private fun getTint(context: Context, color: Color): Int {
        return when (color) {
            Color.BLUE -> getColor(context, R.color.mozac_feature_toolbar_container_blue)
            Color.TURQUOISE -> getColor(context, R.color.mozac_feature_toolbar_container_turquoise)
            Color.GREEN -> getColor(context, R.color.mozac_feature_toolbar_container_green)
            Color.YELLOW -> getColor(context, R.color.mozac_feature_toolbar_container_yellow)
            Color.ORANGE -> getColor(context, R.color.mozac_feature_toolbar_container_orange)
            Color.RED -> getColor(context, R.color.mozac_feature_toolbar_container_red)
            Color.PINK -> getColor(context, R.color.mozac_feature_toolbar_container_pink)
            Color.PURPLE -> getColor(context, R.color.mozac_feature_toolbar_container_purple)
            Color.TOOLBAR -> getColor(context, R.color.mozac_feature_toolbar_container_toolbar)
        }
    }
}
