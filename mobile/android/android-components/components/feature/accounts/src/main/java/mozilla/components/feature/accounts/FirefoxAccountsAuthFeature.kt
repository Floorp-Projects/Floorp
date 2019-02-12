/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.net.Uri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.fxa.FxaAccountManager
import mozilla.components.service.fxa.FxaException
import kotlin.coroutines.CoroutineContext

/**
 * Ties together an account manager with a session manager/tabs implementation, facilitating an
 * authentication flow.
 */
class FirefoxAccountsAuthFeature(
    private val accountManager: FxaAccountManager,
    private val tabsUseCases: TabsUseCases,
    private val redirectUrl: String,
    private val successPath: String,
    private val coroutineContext: CoroutineContext = Dispatchers.Main
) {
    fun beginAuthentication() {
        CoroutineScope(coroutineContext).launch {
            val authUrl = try {
                accountManager.beginAuthenticationAsync().await()
            } catch (e: FxaException) {
                // FIXME return a fallback URL provided by Config...
                "https://accounts.firefox.com/signin"
            }
            // TODO
            // We may fail to obtain an authentication URL, for example due to transient network errors.
            // If that happens, open up a fallback URL in order to present some kind of a "no network"
            // UI to the user.
            // It's possible that the underlying problem will go away by the time the tab actually
            // loads, resulting in a confusing experience.
            tabsUseCases.addTab.invoke(authUrl)
        }
    }

    val interceptor = object : RequestInterceptor {
        override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
            if (!uri.startsWith(redirectUrl)) {
                return null
            }

            val parsedUri = Uri.parse(uri)
            val code = parsedUri.getQueryParameter("code") as String
            val state = parsedUri.getQueryParameter("state") as String

            // Notify the state machine about our success.
            accountManager.finishAuthenticationAsync(code, state)

            // TODO this can be simplified once https://github.com/mozilla/application-services/issues/305 lands
            val successUrl = "${parsedUri.scheme}://${parsedUri.host}/$successPath"
            return RequestInterceptor.InterceptionResponse.Url(successUrl)
        }
    }
}
