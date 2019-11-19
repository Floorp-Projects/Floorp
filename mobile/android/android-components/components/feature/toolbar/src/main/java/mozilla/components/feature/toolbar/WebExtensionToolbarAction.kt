/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.drawable.BitmapDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.android.Padding
import mozilla.components.support.ktx.android.view.setPadding
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * An action button that represents an web extension item to be added to the toolbar.
 *
 * @param browserAction Associated [WebExtensionBrowserAction]
 * @param listener Callback that will be invoked whenever the button is pressed
 */
open class WebExtensionToolbarAction(
    internal var browserAction: WebExtensionBrowserAction,
    internal val padding: Padding? = null,
    internal val listener: () -> Unit
) : Toolbar.Action {
    internal var iconJob: Job? = null

    override fun createView(parent: ViewGroup): View {
        val rootView = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_feature_toolbar_web_extension_action_layout, parent)

        rootView.isEnabled = browserAction.enabled
        rootView.setOnClickListener { listener.invoke() }

        val backgroundResource =
            parent.context.theme.resolveAttribute(android.R.attr.selectableItemBackgroundBorderless)

        rootView.setBackgroundResource(backgroundResource)
        padding?.let { rootView.setPadding(it) }

        parent.addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
            override fun onViewDetachedFromWindow(view: View?) {
                iconJob?.cancel()
            }

            override fun onViewAttachedToWindow(view: View?) = Unit
        })
        return rootView
    }

    override fun bind(view: View) {
        val imageView = view.findViewById<ImageView>(R.id.action_image)
        val textView = view.findViewById<TextView>(R.id.badge_text)

        iconJob = CoroutineScope(Dispatchers.IO).launch {
            val icon = browserAction.loadIcon.invoke(imageView.measuredHeight)

            icon?.let {
                MainScope().launch {
                    imageView.setImageDrawable(BitmapDrawable(view.context.resources, it))
                }
            }
        }

        imageView.contentDescription = browserAction.title
        textView.text = browserAction.badgeText
        textView.setTextColor(browserAction.badgeTextColor)
        textView.setBackgroundColor(browserAction.badgeBackgroundColor)
    }
}
