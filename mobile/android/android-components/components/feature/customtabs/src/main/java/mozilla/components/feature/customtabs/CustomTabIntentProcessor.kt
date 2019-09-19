/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import android.content.res.Resources
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent

/**
 * Processor for intents which trigger actions related to custom tabs.
 */
class CustomTabIntentProcessor(
    private val sessionManager: SessionManager,
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase,
    private val resources: Resources
) : IntentProcessor {

    override fun matches(intent: Intent): Boolean {
        val safeIntent = intent.toSafeIntent()
        return safeIntent.action == ACTION_VIEW && isCustomTabIntent(safeIntent)
    }

    override suspend fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        val url = safeIntent.dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val session = Session(url, private = false, source = Session.Source.CUSTOM_TAB)
            session.customTabConfig = createCustomTabConfigFromIntent(intent, resources)

            sessionManager.add(session)
            loadUrlUseCase(url, session, EngineSession.LoadUrlFlags.external())
            intent.putSessionId(session.id)

            true
        } else {
            false
        }
    }
}
