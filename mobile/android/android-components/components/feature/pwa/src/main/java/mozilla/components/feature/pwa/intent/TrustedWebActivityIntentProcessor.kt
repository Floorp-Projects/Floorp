/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.intent

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import android.content.pm.PackageManager
import android.net.Uri
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsSessionToken
import androidx.core.net.toUri
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabConfig.Companion.EXTRA_ADDITIONAL_TRUSTED_ORIGINS
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.customtabs.createCustomTabConfigFromIntent
import mozilla.components.feature.customtabs.feature.OriginVerifierFeature
import mozilla.components.feature.customtabs.isTrustedWebActivityIntent
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.pwa.ext.toOrigin
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent

/**
 * Processor for intents which open Trusted Web Activities.
 */
class TrustedWebActivityIntentProcessor(
    private val sessionManager: SessionManager,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    httpClient: Client,
    packageManager: PackageManager,
    apiKey: String?,
    private val store: CustomTabsServiceStore
) : IntentProcessor {

    private val verifier = OriginVerifierFeature(httpClient, packageManager, apiKey) { store.dispatch(it) }
    private val scope = MainScope()

    override fun matches(intent: Intent): Boolean {
        val safeIntent = intent.toSafeIntent()
        return safeIntent.action == ACTION_VIEW && isTrustedWebActivityIntent(safeIntent)
    }

    override suspend fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        val url = safeIntent.dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val session = Session(url, private = false, source = Session.Source.HOME_SCREEN)
            val customTabConfig = createCustomTabConfigFromIntent(intent, null)
            session.customTabConfig = customTabConfig

            sessionManager.add(session)
            loadUrlUseCase(url, session, EngineSession.LoadUrlFlags.external())
            intent.putSessionId(session.id)

            customTabConfig.sessionToken?.let { token ->
                val origin = listOfNotNull(safeIntent.data?.toOrigin())
                val additionalOrigins = safeIntent
                    .getStringArrayListExtra(EXTRA_ADDITIONAL_TRUSTED_ORIGINS)
                    .orEmpty()
                    .mapNotNull { it.toUri().toOrigin() }

                // Launch verification separately so the intent processing isn't held up
                scope.launch {
                    verify(token, origin + additionalOrigins)
                }
            }

            true
        } else {
            false
        }
    }

    private suspend fun verify(token: CustomTabsSessionToken, origins: List<Uri>) {
        val tabState = store.state.tabs[token] ?: return
        origins.map { origin ->
            scope.async {
                verifier.verify(tabState, token, RELATION_HANDLE_ALL_URLS, origin)
            }
        }.awaitAll()
    }
}
