/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.trackingprotection.ProtectionsAction
import org.mozilla.fenix.trackingprotection.ProtectionsStore

/**
 * [CookieBannerDetailsController] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it,
 * complex Android interactions or communication with other features.
 */
interface CookieBannerDetailsController {
    /**
     * @see [CookieBannerDetailsInteractor.onBackPressed]
     */
    fun handleBackPressed()

    /**
     * @see [CookieBannerDetailsInteractor.onTogglePressed]
     */
    fun handleTogglePressed(isEnabled: Boolean)
}

/**
 * Default behavior of [CookieBannerDetailsController].
 */
@Suppress("LongParameterList")
class DefaultCookieBannerDetailsController(
    private val context: Context,
    private val fragment: Fragment,
    private val ioScope: CoroutineScope,
    internal val sessionId: String,
    private val browserStore: BrowserStore,
    internal val protectionsStore: ProtectionsStore,
    private val cookieBannersStorage: CookieBannersStorage,
    private val navController: () -> NavController,
    internal var sitePermissions: SitePermissions?,
    private val gravity: Int,
    private val getCurrentTab: () -> SessionState?,
    private val reload: SessionUseCases.ReloadUrlUseCase,
    private val engine: Engine = context.components.core.engine,
    private val publicSuffixList: PublicSuffixList = context.components.publicSuffixList,
) : CookieBannerDetailsController {

    override fun handleBackPressed() {
        getCurrentTab()?.let { tab ->
            context.components.useCases.trackingProtectionUseCases.containsException(tab.id) { contains ->
                ioScope.launch {
                    val hasException =
                        cookieBannersStorage.hasException(tab.content.url, tab.content.private)
                    withContext(Dispatchers.Main) {
                        fragment.runIfFragmentIsAttached {
                            navController().popBackStack()
                            val isTrackingProtectionEnabled = tab.trackingProtection.enabled && !contains
                            val directions =
                                BrowserFragmentDirections.actionGlobalQuickSettingsSheetDialogFragment(
                                    sessionId = tab.id,
                                    url = tab.content.url,
                                    title = tab.content.title,
                                    isSecured = tab.content.securityInfo.secure,
                                    sitePermissions = sitePermissions,
                                    gravity = gravity,
                                    certificateName = tab.content.securityInfo.issuer,
                                    permissionHighlights = tab.content.permissionHighlights,
                                    isTrackingProtectionEnabled = isTrackingProtectionEnabled,
                                    isCookieHandlingEnabled = !hasException,
                                )
                            navController().navigate(directions)
                        }
                    }
                }
            }
        }
    }

    override fun handleTogglePressed(isEnabled: Boolean) {
        val tab = requireNotNull(browserStore.state.findTabOrCustomTab(sessionId)) {
            "A session is required to update the cookie banner mode"
        }
        ioScope.launch {
            if (isEnabled) {
                cookieBannersStorage.removeException(
                    uri = tab.content.url,
                    privateBrowsing = tab.content.private,
                )
                CookieBanners.exceptionRemoved.record(NoExtras())
            } else {
                clearSiteData(tab)
                cookieBannersStorage.addException(uri = tab.content.url, privateBrowsing = tab.content.private)
                CookieBanners.exceptionAdded.record(NoExtras())
            }
            protectionsStore.dispatch(
                ProtectionsAction.ToggleCookieBannerHandlingProtectionEnabled(
                    isEnabled,
                ),
            )
            reload(tab.id)
        }
    }

    @VisibleForTesting
    internal suspend fun clearSiteData(tab: SessionState) {
        val host = tab.content.url.toUri().host.orEmpty()
        val domain = publicSuffixList.getPublicSuffixPlusOne(host).await()
        withContext(Dispatchers.Main) {
            engine.clearData(
                host = domain,
                data = Engine.BrowsingData.select(
                    Engine.BrowsingData.AUTH_SESSIONS,
                    Engine.BrowsingData.ALL_SITE_DATA,
                ),
            )
        }
    }
}
