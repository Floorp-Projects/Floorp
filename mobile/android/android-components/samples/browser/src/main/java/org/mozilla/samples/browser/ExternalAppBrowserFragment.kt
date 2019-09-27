/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.customtabs.CustomTabWindowFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.pwa.ext.getTrustedScope
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.feature.pwa.ext.putWebAppManifest
import mozilla.components.feature.pwa.ext.trustedOrigins
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.feature.pwa.feature.WebAppSiteControlsFeature
import mozilla.components.lib.state.ext.consumeFrom
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
                listOfNotNull(manifest?.getTrustedScope())
            ),
            owner = this,
            view = layout.toolbar)

        val windowFeature = CustomTabWindowFeature(
            requireContext(),
            components.sessionManager,
            sessionId!!
        )
        lifecycle.addObserver(windowFeature)

        if (manifest != null) {
            activity?.lifecycle?.addObserver(
                WebAppActivityFeature(
                    activity!!,
                    components.icons,
                    manifest
                )
            )
            activity?.lifecycle?.addObserver(
                WebAppSiteControlsFeature(
                    context?.applicationContext!!,
                    components.sessionManager,
                    components.sessionUseCases.reload,
                    sessionId!!,
                    manifest
                )
            )
        }

        return layout
    }

    @ObsoleteCoroutinesApi
    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val scope = listOfNotNull(manifest?.getTrustedScope())

        consumeFrom(components.customTabsStore) { state ->
            sessionId
                ?.let { components.sessionManager.findSessionById(it) }
                ?.let { session -> session.customTabConfig?.sessionToken }
                ?.let { token -> state.tabs[token] }
                ?.let { tabState ->
                    hideToolbarFeature.withFeature {
                        it.onTrustedScopesChange(scope + tabState.trustedOrigins)
                    }
                }
        }
    }

    /**
     * Calls [onBackPressed] for features in the base class first,
     * before trying to call the external app [BackHandler].
     */
    override fun onBackPressed(): Boolean =
        super.onBackPressed() || customTabsToolbarFeature.onBackPressed()

    companion object {
        fun create(
            sessionId: String,
            manifest: WebAppManifest?
        ) = ExternalAppBrowserFragment().apply {
            arguments = Bundle().apply {
                putSessionId(sessionId)
                putWebAppManifest(manifest)
            }
        }
    }
}
