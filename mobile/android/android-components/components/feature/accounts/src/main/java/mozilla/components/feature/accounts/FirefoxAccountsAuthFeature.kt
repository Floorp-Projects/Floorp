/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.toAuthType
import kotlin.coroutines.CoroutineContext

/**
 * Ties together an account manager with a session manager/tabs implementation, facilitating an
 * authentication flow.
 * @property accountManager [FxaAccountManager].
 * @property redirectUrl This is the url that will be reached at the final authentication state.
 * @property coroutineContext The context which will be used to execute network requests necessary
 * to initiate authentication. Note that [onBeginAuthentication] will be executed using this context.
 * @property onBeginAuthentication A lambda function that receives the authentication url.
 * Executed on [coroutineContext].
 */
class FirefoxAccountsAuthFeature(
    private val accountManager: FxaAccountManager,
    private val redirectUrl: String,
    private val coroutineContext: CoroutineContext = Dispatchers.IO,
    private val onBeginAuthentication: (Context, String) -> Unit = { _, _ -> },
) {
    fun beginAuthentication(context: Context) {
        beginAuthenticationAsync(context) {
            accountManager.beginAuthentication()
        }
    }

    fun beginPairingAuthentication(context: Context, pairingUrl: String) {
        beginAuthenticationAsync(context) {
            accountManager.beginAuthentication(pairingUrl)
        }
    }

    private fun beginAuthenticationAsync(context: Context, beginAuthentication: suspend () -> String?) {
        CoroutineScope(coroutineContext).launch {
            // FIXME return a fallback URL provided by Config...
            // https://github.com/mozilla-mobile/android-components/issues/2496
            val authUrl = beginAuthentication() ?: "https://accounts.firefox.com/signin"

            // TODO
            // We may fail to obtain an authentication URL, for example due to transient network errors.
            // If that happens, open up a fallback URL in order to present some kind of a "no network"
            // UI to the user.
            // It's possible that the underlying problem will go away by the time the tab actually
            // loads, resulting in a confusing experience.

            onBeginAuthentication(context, authUrl)
        }
    }

    val interceptor = object : RequestInterceptor {
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
            if (uri.startsWith(redirectUrl)) {
                val parsedUri = Uri.parse(uri)
                val code = parsedUri.getQueryParameter("code")

                if (code != null) {
                    val authType = parsedUri.getQueryParameter("action").toAuthType()
                    val state = parsedUri.getQueryParameter("state") as String

                    // Notify the state machine about our success.
                    CoroutineScope(Dispatchers.Main).launch {
                        accountManager.finishAuthentication(
                            FxaAuthData(
                                authType = authType,
                                code = code,
                                state = state,
                            ),
                        )
                    }

                    return RequestInterceptor.InterceptionResponse.Url(redirectUrl)
                }
            }

            return null
        }
    }
}
