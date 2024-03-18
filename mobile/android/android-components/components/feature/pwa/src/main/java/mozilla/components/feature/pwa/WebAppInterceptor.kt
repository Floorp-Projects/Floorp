/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import android.content.Intent
import android.net.Uri
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.feature.pwa.ext.putUrlOverride
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor

/**
 * This feature will intercept requests and reopen them in the corresponding installed PWA, if any.
 *
 * @param shortcutManager current shortcut manager instance to lookup web app install states
 */
class WebAppInterceptor(
    private val context: Context,
    private val manifestStorage: ManifestStorage,
    private val launchFromInterceptor: Boolean = true,
) : RequestInterceptor {

    @Suppress("ReturnCount")
    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        val scope = manifestStorage.getInstalledScope(uri) ?: return null
        val startUrl = manifestStorage.getStartUrlForInstalledScope(scope) ?: return null
        val intent = createIntentFromUri(startUrl, uri)

        if (!launchFromInterceptor) {
            return RequestInterceptor.InterceptionResponse.AppIntent(intent, uri)
        }

        intent.flags = intent.flags or Intent.FLAG_ACTIVITY_NEW_TASK
        context.startActivity(intent)

        return RequestInterceptor.InterceptionResponse.Deny
    }

    /**
     * Creates a new VIEW_PWA intent for a URL.
     *
     * @param uri target URL for the new intent
     */
    private fun createIntentFromUri(startUrl: String, urlOverride: String = startUrl): Intent {
        return Intent(WebAppIntentProcessor.ACTION_VIEW_PWA, Uri.parse(startUrl)).apply {
            this.addCategory(Intent.CATEGORY_DEFAULT)
            this.putUrlOverride(urlOverride)
        }
    }
}
