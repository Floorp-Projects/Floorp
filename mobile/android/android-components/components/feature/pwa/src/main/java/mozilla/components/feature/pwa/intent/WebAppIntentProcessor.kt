/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.intent

import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.ext.getUrlOverride
import mozilla.components.feature.pwa.ext.putWebAppManifest
import mozilla.components.feature.pwa.ext.toCustomTabConfig
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.utils.toSafeIntent

/**
 * Processor for intents which trigger actions related to web apps.
 */
class WebAppIntentProcessor(
    private val store: BrowserStore,
    private val addTabUseCase: CustomTabsUseCases.AddWebAppTabUseCase,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    private val storage: ManifestStorage,
) : IntentProcessor {

    /**
     * Returns true if this intent should launch a progressive web app.
     */
    private fun matches(intent: Intent) =
        intent.toSafeIntent().action == ACTION_VIEW_PWA

    /**
     * Processes the given [Intent] by creating a [Session] with a corresponding web app manifest.
     *
     * A custom tab config is also set so a custom tab toolbar can be shown when the user leaves
     * the scope defined in the manifest.
     */
    override fun process(intent: Intent): Boolean {
        val url = intent.toSafeIntent().dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val webAppManifest = runBlocking { storage.loadManifest(url) } ?: return false
            val targetUrl = intent.getUrlOverride() ?: url

            val id = findExistingSession(webAppManifest) ?: createSession(webAppManifest, url)

            if (targetUrl !== url) {
                loadUrlUseCase(targetUrl, id, EngineSession.LoadUrlFlags.external())
            }

            intent.flags = FLAG_ACTIVITY_NEW_DOCUMENT
            intent.putSessionId(id)
            intent.putWebAppManifest(webAppManifest)

            true
        } else {
            false
        }
    }

    /**
     * Returns an existing web app session that matches the manifest.
     */
    private fun findExistingSession(webAppManifest: WebAppManifest): String? {
        return store.state.customTabs.find { tab ->
            tab.config.externalAppType == ExternalAppType.PROGRESSIVE_WEB_APP &&
                tab.content.webAppManifest?.startUrl == webAppManifest.startUrl
        }?.id
    }

    /**
     * Returns a new web app session.
     */
    private fun createSession(webAppManifest: WebAppManifest, url: String): String {
        return addTabUseCase.invoke(
            url = url,
            source = Source.Internal.HomeScreen,
            webAppManifest = webAppManifest,
            customTabConfig = webAppManifest.toCustomTabConfig(),
        )
    }

    companion object {
        const val ACTION_VIEW_PWA = "mozilla.components.feature.pwa.VIEW_PWA"
    }
}
