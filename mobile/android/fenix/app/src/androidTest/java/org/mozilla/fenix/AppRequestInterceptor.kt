/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.TestHelper.appContext
import java.lang.ref.WeakReference

/**
 * This class overrides the application's request interceptor to
 * deactivate the FxA web channel
 * which is not supported on the staging servers.
 */
class AppRequestInterceptor(private val context: Context) : RequestInterceptor {

    private var navController: WeakReference<NavController>? = null

    fun setNavigationController(navController: NavController) {
        this.navController = WeakReference(navController)
    }

    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean
    ): RequestInterceptor.InterceptionResponse? {
        return appContext.components.services.accountsAuthFeature.interceptor.onLoadRequest(
            engineSession, uri, lastUri, hasUserGesture, isSameDomain, isRedirect, isDirectNavigation, isSubframeRequest
        )
    }
}
