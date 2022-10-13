/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature used to update the existing web app manifest and web app shortcut.
 *
 * @param shortcutManager Shortcut manager used to update pinned shortcuts.
 * @param storage Manifest storage used to have updated manifests.
 * @param sessionId ID of the web app session to observe.
 * @param initialManifest Loaded manifest for the current web app.
 */
@Suppress("LongParameterList")
class ManifestUpdateFeature(
    private val applicationContext: Context,
    private val store: BrowserStore,
    private val shortcutManager: WebAppShortcutManager,
    private val storage: ManifestStorage,
    private val sessionId: String,
    private var initialManifest: WebAppManifest,
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var updateJob: Job? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var updateUsageJob: Job? = null

    /**
     * Updates the manifest on disk then updates the pinned shortcut to reflect changes.
     */
    @VisibleForTesting
    internal suspend fun updateStoredManifest(manifest: WebAppManifest) {
        storage.updateManifest(manifest)
        shortcutManager.updateShortcuts(applicationContext, listOf(manifest))
        initialManifest = manifest
    }

    override fun start() {
        scope = MainScope().also { observeManifestChanges(it) }
        updateUsageJob?.cancel()

        updateUsageJob = scope?.launch {
            storage.updateManifestUsedAt(initialManifest)
        }
    }

    private fun observeManifestChanges(scope: CoroutineScope) = scope.launch {
        store.flow()
            .mapNotNull { state -> state.findCustomTab(sessionId) }
            .map { tab -> tab.content.webAppManifest }
            .ifChanged()
            .collect { manifest -> onWebAppManifestChanged(manifest) }
    }

    override fun stop() {
        scope?.cancel()
    }

    /**
     * When the manifest is changed, compare it to the existing manifest.
     * If it is different, update the disk and shortcut. Ignore if called with a null
     * manifest or a manifest with a different start URL.
     */
    private fun onWebAppManifestChanged(manifest: WebAppManifest?) {
        if (manifest?.startUrl == initialManifest.startUrl && manifest != initialManifest) {
            updateJob?.cancel()
            updateUsageJob?.cancel()

            updateJob = scope?.launch { updateStoredManifest(manifest) }
        }
    }
}
