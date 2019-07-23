/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.samples.browser.ext.components

class WebAppBrowserFragment : BaseBrowserFragment(), BackHandler {
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val layout = super.onCreateView(inflater, container, savedInstanceState)

        customTabsToolbarFeature.set(
            feature = CustomTabsToolbarFeature(
                components.sessionManager,
                layout.toolbar,
                sessionId,
                components.menuBuilder,
                window = activity?.window,
                closeListener = { activity?.finish() }),
            owner = this,
            view = layout)

        return layout
    }

    /**
     * Calls [onBackPressed] for features in the base class first,
     * before trying to call the progressive web app [BackHandler].
     */
    override fun onBackPressed(): Boolean =
        super.onBackPressed() || customTabsToolbarFeature.onBackPressed()

    companion object {
        fun create(sessionId: String) = WebAppBrowserFragment().apply {
            arguments = Bundle().apply {
                putSessionId(sessionId)
            }
        }
    }
}
