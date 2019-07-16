/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.feature.pwa.ext.putWebAppManifest
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.samples.browser.ext.components

/**
 * Fragment used for browsing within an external app, such as for custom tabs and PWAs.
 */
class ExternalAppBrowserFragment : BaseBrowserFragment(), BackHandler {
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()
    private val hideToolbarFeature = ViewBoundFeatureWrapper<WebAppHideToolbarFeature>()

    private val manifest: WebAppManifest?
        get() = arguments?.getWebAppManifest()
    private val trustedScopes: List<Uri>
        get() = arguments?.getParcelableArrayList<Uri>(ARG_TRUSTED_SCOPES).orEmpty()

    @Suppress("LongMethod")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val layout = super.onCreateView(inflater, container, savedInstanceState)

        val manifest = this.manifest

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

        hideToolbarFeature.set(
            feature = WebAppHideToolbarFeature(
                components.sessionManager,
                layout.toolbar,
                sessionId!!,
                trustedScopes
            ),
            owner = this,
            view = layout.toolbar)

        if (manifest != null) {
            activity?.lifecycle?.addObserver(
                WebAppActivityFeature(
                    activity!!,
                    components.icons,
                    manifest
                )
            )
        }

        return layout
    }

    /**
     * Calls [onBackPressed] for features in the base class first,
     * before trying to call the external app [BackHandler].
     */
    override fun onBackPressed(): Boolean =
        super.onBackPressed() || customTabsToolbarFeature.onBackPressed()

    companion object {
        private const val ARG_TRUSTED_SCOPES = "org.mozilla.samples.browser.TRUSTED_SCOPES"

        fun create(
            sessionId: String,
            manifest: WebAppManifest?,
            trustedScopes: List<Uri>
        ) = ExternalAppBrowserFragment().apply {
            arguments = Bundle().apply {
                putSessionId(sessionId)
                putWebAppManifest(manifest)
                putParcelableArrayList(ARG_TRUSTED_SCOPES, ArrayList(trustedScopes))
            }
        }
    }
}
