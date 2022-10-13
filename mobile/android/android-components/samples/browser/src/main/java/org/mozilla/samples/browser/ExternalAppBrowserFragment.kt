/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.customtabs.CustomTabWindowFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.feature.pwa.ext.putWebAppManifest
import mozilla.components.feature.pwa.feature.ManifestUpdateFeature
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.feature.pwa.feature.WebAppSiteControlsFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.samples.browser.ext.components

/**
 * Fragment used for browsing within an external app, such as for custom tabs and PWAs.
 */
class ExternalAppBrowserFragment : BaseBrowserFragment(), UserInteractionHandler {
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()
    private val hideToolbarFeature = ViewBoundFeatureWrapper<WebAppHideToolbarFeature>()

    private val manifest: WebAppManifest?
        get() = arguments?.getWebAppManifest()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        super.onCreateView(inflater, container, savedInstanceState)
        val binding = super.binding

        val manifest = this.manifest

        customTabsToolbarFeature.set(
            feature = CustomTabsToolbarFeature(
                components.store,
                binding.toolbar,
                sessionId,
                components.customTabsUseCases,
                components.menuBuilder,
                window = activity?.window,
                closeListener = { activity?.finish() },
            ),
            owner = this,
            view = binding.root,
        )

        hideToolbarFeature.set(
            feature = WebAppHideToolbarFeature(
                components.store,
                components.customTabsStore,
                sessionId,
                manifest,
            ) { toolbarVisible ->
                binding.toolbar.isVisible = toolbarVisible
            },
            owner = this,
            view = binding.toolbar,
        )

        val windowFeature = CustomTabWindowFeature(
            requireActivity(),
            components.store,
            sessionId!!,
        ) {
            // No-op. Client may override this
        }
        lifecycle.addObserver(windowFeature)

        if (manifest != null) {
            activity?.lifecycle?.addObservers(
                WebAppActivityFeature(
                    requireActivity(),
                    components.icons,
                    manifest,
                ),
                ManifestUpdateFeature(
                    requireContext(),
                    components.store,
                    components.webAppShortcutManager,
                    components.webAppManifestStorage,
                    sessionId!!,
                    manifest,
                ),
            )
            viewLifecycleOwner.lifecycle.addObserver(
                WebAppSiteControlsFeature(
                    context?.applicationContext!!,
                    components.store,
                    components.sessionUseCases.reload,
                    sessionId!!,
                    manifest,
                    icons = components.icons,
                ),
            )
        }

        return binding.root
    }

    /**
     * Calls [onBackPressed] for features in the base class first,
     * before trying to call the external app [UserInteractionHandler].
     */
    override fun onBackPressed(): Boolean =
        super.onBackPressed() || customTabsToolbarFeature.onBackPressed()

    companion object {
        fun create(
            sessionId: String,
            manifest: WebAppManifest?,
        ) = ExternalAppBrowserFragment().apply {
            arguments = Bundle().apply {
                putSessionId(sessionId)
                putWebAppManifest(manifest)
            }
        }
    }
}
