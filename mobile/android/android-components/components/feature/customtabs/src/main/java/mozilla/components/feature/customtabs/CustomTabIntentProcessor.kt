/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import android.content.res.Resources
import android.provider.Browser
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.externalPackage
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.toSafeIntent

/**
 * Processor for intents which trigger actions related to custom tabs.
 */
class CustomTabIntentProcessor(
    private val addCustomTabUseCase: CustomTabsUseCases.AddCustomTabUseCase,
    private val resources: Resources,
    private val isPrivate: Boolean = false,
) : IntentProcessor {

    private fun matches(intent: Intent): Boolean {
        val safeIntent = intent.toSafeIntent()
        return safeIntent.action == ACTION_VIEW && isCustomTabIntent(safeIntent)
    }

    @VisibleForTesting
    internal fun getAdditionalHeaders(intent: SafeIntent): Map<String, String>? {
        val pairs = intent.getBundleExtra(Browser.EXTRA_HEADERS)
        val headers = mutableMapOf<String, String>()
        pairs?.keySet()?.forEach { key ->
            val header = pairs.getString(key)
            if (header != null) {
                headers[key] = header
            } else {
                throw IllegalArgumentException("getAdditionalHeaders() intent bundle contains wrong key value pair")
            }
        }
        return if (headers.isEmpty()) {
            null
        } else {
            headers
        }
    }

    override fun process(intent: Intent): Boolean {
        val safeIntent = SafeIntent(intent)
        val url = safeIntent.dataString

        return if (!url.isNullOrEmpty() && matches(intent)) {
            val config = createCustomTabConfigFromIntent(intent, resources)
            val caller = safeIntent.externalPackage()
            val customTabId = addCustomTabUseCase(
                url,
                config,
                isPrivate,
                getAdditionalHeaders(safeIntent),
                source = SessionState.Source.External.CustomTab(caller),
            )
            intent.putSessionId(customTabId)

            true
        } else {
            false
        }
    }
}
