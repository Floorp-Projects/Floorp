/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import androidx.core.content.pm.ShortcutManagerCompat
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.pwa.ext.installableManifest

/**
 * These use cases allow for adding a web app or web site to the homescreen.
 */
class WebAppUseCases(
    private val applicationContext: Context,
    private val store: BrowserStore,
    private val shortcutManager: WebAppShortcutManager,
) {
    /**
     * Checks if the launcher supports adding shortcuts.
     */
    fun isPinningSupported() =
        ShortcutManagerCompat.isRequestPinShortcutSupported(applicationContext)

    /**
     * Checks to see if the current session can be installed as a Progressive Web App.
     */
    fun isInstallable() =
        store.state.selectedTab?.installableManifest() != null && shortcutManager.supportWebApps

    /**
     * Let the user add the selected session to the homescreen.
     *
     * If the selected session represents a Progressive Web App, then the
     * manifest will be saved and the web app will be launched based on the
     * manifest values.
     *
     * Otherwise, the pinned shortcut will act like a simple bookmark for the site.
     */
    class AddToHomescreenUseCase internal constructor(
        private val applicationContext: Context,
        private val store: BrowserStore,
        private val shortcutManager: WebAppShortcutManager,
    ) {

        /**
         * @param overrideBasicShortcutName (optional) Custom label used if the current session
         * is NOT a Progressive Web App
         */
        suspend operator fun invoke(overrideBasicShortcutName: String? = null) {
            val session = store.state.selectedTab ?: return
            shortcutManager.requestPinShortcut(applicationContext, session, overrideBasicShortcutName)
        }
    }

    val addToHomescreen by lazy {
        AddToHomescreenUseCase(applicationContext, store, shortcutManager)
    }

    /**
     * Checks the current install state of a Web App.
     *
     * Returns WebAppShortcutManager.InstallState.Installed if the user has installed
     * or used the web app in the past 30 days.
     *
     * Otherwise, WebAppShortcutManager.InstallState.NotInstalled is returned.
     */
    class GetInstallStateUseCase internal constructor(
        private val store: BrowserStore,
        private val shortcutManager: WebAppShortcutManager,
    ) {
        /**
         * @param currentTimeMs the current time against which manifest usage timeouts will be validated
         */
        suspend operator fun invoke(
            currentTimeMs: Long = System.currentTimeMillis(),
        ): WebAppShortcutManager.WebAppInstallState? {
            val session = store.state.selectedTab ?: return null
            return shortcutManager.getWebAppInstallState(session.content.url, currentTimeMs)
        }
    }

    val getInstallState by lazy {
        GetInstallStateUseCase(store, shortcutManager)
    }
}
