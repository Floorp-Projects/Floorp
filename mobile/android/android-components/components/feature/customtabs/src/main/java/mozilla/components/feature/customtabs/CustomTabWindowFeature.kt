/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabsIntent
import androidx.core.net.toUri
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation for handling window requests by opening custom tabs.
 */
class CustomTabWindowFeature(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String
) : LifecycleAwareFeature {

    @VisibleForTesting
    internal val windowObserver = object : Session.Observer {
        override fun onOpenWindowRequested(session: Session, windowRequest: WindowRequest): Boolean {
            val intent = configToIntent(session.customTabConfig)
            intent.launchUrl(context, windowRequest.url.toUri())
            return true
        }
    }

    /**
     * Transform a [CustomTabConfig] into a [CustomTabsIntent] that creates a
     * new custom tab with the same styling and layout
     */
    @Suppress("ComplexMethod")
    @VisibleForTesting
    internal fun configToIntent(config: CustomTabConfig?): CustomTabsIntent {
        val intent = CustomTabsIntent.Builder().apply {
            setInstantAppsEnabled(false)
            config?.toolbarColor?.let { setToolbarColor(it) }
            config?.navigationBarColor?.let { setNavigationBarColor(it) }
            if (config?.enableUrlbarHiding == true) enableUrlBarHiding()
            config?.closeButtonIcon?.let { setCloseButtonIcon(it) }
            if (config?.showShareMenuItem == true) addDefaultShareMenuItem()
            config?.titleVisible?.let { setShowTitle(it) }
            config?.actionButtonConfig?.apply { setActionButton(icon, description, pendingIntent, tint) }
            config?.menuItems?.forEach { addMenuItem(it.name, it.pendingIntent) }
        }.build()

        intent.intent.`package` = context.packageName

        return intent
    }

    /**
     * Starts the feature and a observer to listen for window requests.
     */
    override fun start() {
        sessionManager.findSessionById(sessionId)?.register(windowObserver)
    }

    /**
     * Stops the feature and the window request observer.
     */
    override fun stop() {
        sessionManager.findSessionById(sessionId)?.unregister(windowObserver)
    }
}
