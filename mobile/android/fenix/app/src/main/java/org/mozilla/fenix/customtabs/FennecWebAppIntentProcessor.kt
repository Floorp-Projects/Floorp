/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.concept.engine.manifest.getOrNull
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.ext.putWebAppManifest
import mozilla.components.feature.pwa.ext.toCustomTabConfig
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.toSafeIntent
import org.json.JSONObject
import java.io.File

/**
 * Legacy processor for Progressive Web App shortcut intents created by Fennec.
 */
class FennecWebAppIntentProcessor(
    private val sessionManager: SessionManager,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    private val storage: ManifestStorage
) : IntentProcessor {

    /**
     * Returns true if this intent should launch a progressive web app created in Fennec.
     */
    override fun matches(intent: Intent) =
        intent.toSafeIntent().action == ACTION_FENNEC_WEBAPP

    /**
     * Processes the given [Intent] by creating a [Session] with a corresponding web app manifest.
     *
     * A custom tab config is also set so a custom tab toolbar can be shown when the user leaves
     * the scope defined in the manifest.
     */
    override suspend fun process(intent: Intent): Boolean {
        val safeIntent = intent.toSafeIntent()
        val url = safeIntent.dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val webAppManifest = storage.loadManifest(url)
                ?: fromFile(safeIntent.getStringExtra(EXTRA_FENNEC_MANIFEST_PATH))?.also {
                    storage.saveManifest(it)
                }
                ?: return false

            val session = Session(url, private = false, source = Source.HOME_SCREEN)
            session.webAppManifest = webAppManifest
            session.customTabConfig = webAppManifest.toCustomTabConfig()

            sessionManager.add(session)
            loadUrlUseCase(url, session, EngineSession.LoadUrlFlags.external())
            intent.flags = FLAG_ACTIVITY_NEW_DOCUMENT
            intent.putSessionId(session.id)
            intent.putWebAppManifest(webAppManifest)

            true
        } else {
            false
        }
    }

    @VisibleForTesting
    internal fun fromFile(path: String?): WebAppManifest? {
        if (path.isNullOrEmpty()) return null

        // Gecko in Fennec added some add some additional data, such as cached_icon, in
        // the toplevel object. The actual web app manifest is in the "manifest" field.
        val manifest = JSONObject(File(path).readText())
        val manifestField = manifest.getJSONObject("manifest")

        return WebAppManifestParser().parse(manifestField).getOrNull()
    }

    companion object {
        const val ACTION_FENNEC_WEBAPP = "org.mozilla.gecko.WEBAPP"
        const val EXTRA_FENNEC_MANIFEST_PATH = "MANIFEST_PATH"
    }
}
