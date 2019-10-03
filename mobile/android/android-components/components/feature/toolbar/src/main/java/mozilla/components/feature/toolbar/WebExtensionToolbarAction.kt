/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.android.Padding
import mozilla.components.support.ktx.android.view.setPadding
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * An action button that represents an web extension item to be added to the toolbar.
 *
 * @param imageDrawable The drawable to be shown.
 * @param contentDescription The content description to use.
 * @param enabled Indicates whether this button is enabled or not.
 * @param badgeText The text shown above of the [imageDrawable].
 * @param badgeTextColor The color of the [badgeText].
 * @param badgeBackgroundColor The background color of the badge.
 * @param padding A optional custom padding.
 * @param listener Callback that will be invoked whenever the button is pressed
 */
open class WebExtensionToolbarAction(
    internal val imageDrawable: Drawable,
    internal val contentDescription: String,
    internal val enabled: Boolean,
    internal val badgeText: String,
    internal val badgeTextColor: Int = 0,
    internal val badgeBackgroundColor: Int = 0,
    internal val padding: Padding? = null,
    internal val listener: () -> Unit
) : Toolbar.Action {

    override fun createView(parent: ViewGroup): View {
        val rootView = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_feature_toolbar_web_extension_action_layout, parent)

        rootView.isEnabled = enabled
        rootView.setOnClickListener { listener.invoke() }

        val backgroundResource =
            parent.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)

        rootView.setBackgroundResource(backgroundResource)
        padding?.let { rootView.setPadding(it) }
        return rootView
    }

    override fun bind(view: View) {
        val imageView = view.findViewById<ImageView>(R.id.action_image)
        val textView = view.findViewById<TextView>(R.id.badge_text)

        imageView.setImageDrawable(imageDrawable)
        imageView.contentDescription = contentDescription

        textView.text = badgeText
        textView.setTextColor(badgeTextColor)
        textView.setBackgroundColor(badgeBackgroundColor)
    }
}
