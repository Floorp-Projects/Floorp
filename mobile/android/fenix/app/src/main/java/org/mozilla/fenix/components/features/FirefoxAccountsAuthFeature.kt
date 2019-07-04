package org.mozilla.fenix.components.features

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.settings.SupportUtils
import kotlin.coroutines.CoroutineContext

// The FirefoxAccountsAuthFeature provided by android components only worked with
// the tabs use case. We want to use our custom tab feature to improve the UX for both
// signing in through settings and onboarding.

// We're temporarily copying and creating our own:
// https://github.com/mozilla-mobile/android-components/issues/3272
class FirefoxAccountsAuthFeature(
    private val accountManager: FxaAccountManager,
    private val redirectUrl: String,
    private val coroutineContext: CoroutineContext = Dispatchers.Main
) {
    fun beginAuthentication(context: Context) {
        beginAuthenticationAsync(context) {
            accountManager.beginAuthenticationAsync().await()
        }
    }

    fun beginPairingAuthentication(context: Context, pairingUrl: String) {
        beginAuthenticationAsync(context) {
            accountManager.beginAuthenticationAsync(pairingUrl).await()
        }
    }

    private fun beginAuthenticationAsync(context: Context, beginAuthentication: suspend () -> String?) {
        CoroutineScope(coroutineContext).launch {
            // FIXME return a fallback URL provided by Config...
            // https://github.com/mozilla-mobile/android-components/issues/2496
            val authUrl = beginAuthentication() ?: FALLBACK_URL

            // TODO
            // We may fail to obtain an authentication URL, for example due to transient network errors.
            // If that happens, open up a fallback URL in order to present some kind of a "no network"
            // UI to the user.
            // It's possible that the underlying problem will go away by the time the tab actually
            // loads, resulting in a confusing experience.
            val intent = SupportUtils.createAuthCustomTabIntent(context, authUrl)
            context.startActivity(intent)
        }
    }

    val interceptor = object : RequestInterceptor {
        override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
            if (uri.startsWith(redirectUrl)) {
                val parsedUri = Uri.parse(uri)
                val code = parsedUri.getQueryParameter("code")

                if (code != null) {
                    val state = parsedUri.getQueryParameter("state") as String

                    // Notify the state machine about our success.
                    accountManager.finishAuthenticationAsync(code, state)

                    return RequestInterceptor.InterceptionResponse.Url(redirectUrl)
                }
            }

            return null
        }
    }

    companion object {
        private const val FALLBACK_URL = "https://accounts.firefox.com/signin"
    }
}
