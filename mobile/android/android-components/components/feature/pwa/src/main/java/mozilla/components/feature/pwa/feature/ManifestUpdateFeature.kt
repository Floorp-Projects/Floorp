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
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature used to update the existing web app manifest and web app shortcut.
 *
 * @param shortcutManager Shortcut manager used to update pinned shortcuts.
 * @param storage Manifest storage used to have updated manifests.
 * @param sessionId ID of the web app session to observe.
 * @param initialManifest Loaded manifest for the current web app.
 */
class ManifestUpdateFeature(
    private val applicationContext: Context,
    private val sessionManager: SessionManager,
    private val shortcutManager: WebAppShortcutManager,
    private val storage: ManifestStorage,
    private val sessionId: String,
    private var initialManifest: WebAppManifest
) : Session.Observer, LifecycleAwareFeature {

    private var scope: CoroutineScope? = null
    private var updateJob: Job? = null

    /**
     * When the manifest is changed, compare it to the existing manifest.
     * If it is different, update the disk and shortcut. Ignore if called with a null
     * manifest or a manifest with a different start URL.
     */
    override fun onWebAppManifestChanged(session: Session, manifest: WebAppManifest?) {
        if (manifest?.startUrl == initialManifest.startUrl && manifest != initialManifest) {
            updateJob?.cancel()
            updateJob = scope?.launch { updateStoredManifest(manifest) }
        }
    }

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
        scope = MainScope()
        sessionManager.findSessionById(sessionId)?.register(this)
    }

    override fun stop() {
        scope?.cancel()
        sessionManager.findSessionById(sessionId)?.unregister(this)
    }
}
