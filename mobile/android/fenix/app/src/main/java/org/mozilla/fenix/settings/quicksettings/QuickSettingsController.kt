/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.ext.shouldBeEnabled
import org.mozilla.fenix.settings.toggle
import org.mozilla.fenix.utils.Settings

/**
 * [QuickSettingsSheetDialogFragment] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it,
 * complex Android interactions or communication with other features.
 */
interface QuickSettingsController {
    /**
     * Handles the case of the [WebsitePermissionsView] needed to be displayed to the user.
     */
    fun handlePermissionsShown()

    /**
     * Handles toggling a [WebsitePermission].
     *
     * @param permission [WebsitePermission] needing to be toggled.
     */
    fun handlePermissionToggled(permission: WebsitePermission)

    /**
     * Handles change a [WebsitePermission.Autoplay].
     *
     * @param autoplayValue [AutoplayValue] needing to be changed.
     */
    fun handleAutoplayChanged(autoplayValue: AutoplayValue)

    /**
     * Handles a certain set of Android permissions being explicitly granted by the user.
     *
     * feature [PhoneFeature] which the user granted Android permission(s) for.
     */
    fun handleAndroidPermissionGranted(feature: PhoneFeature)

    /**
     * @see [ProtectionsInteractor.onTrackingProtectionToggled]
     */
    fun handleTrackingProtectionToggled(isEnabled: Boolean)

    /**
     * Navigates to the cookie banners details panel.
     */
    fun handleCookieBannerHandlingDetailsClicked()

    /**
     * Navigates to the tracking protection details panel.
     */
    fun handleTrackingProtectionDetailsClicked()

    /**
     * Navigates to the connection details. Called when a user clicks on the
     * "Secured or Insecure Connection" section.
     */
    fun handleConnectionDetailsClicked()

    /**
     * Clears site data for the current website. Called when a user clicks
     * on the section to clear site data.
     */
    fun handleClearSiteDataClicked(baseDomain: String)
}

/**
 * Default behavior of [QuickSettingsController]. Other implementations are possible.
 *
 * @property context [Context] used for various Android interactions.
 * @property quickSettingsStore [QuickSettingsFragmentStore] holding the State for all Views displayed
 * in this Controller's Fragment.
 * @property browserStore The application's [BrowserStore].
 * @property ioScope [CoroutineScope] with an IO dispatcher used for structured concurrency.
 * @property navController NavController] used for navigation.
 * @property sessionId ID of the session to manipulate.
 * @property sitePermissions [SitePermissions]? list of website permissions and their status.
 * @property settings [Settings] application settings.
 * @property permissionStorage [PermissionStorage] app state for website permissions exception.
 * @property reload [ReloadUrlUseCase] callback allowing for reloading the current web page.
 * @property requestRuntimePermissions [OnNeedToRequestPermissions] callback allowing for requesting
 * specific Android runtime permissions.
 * @property displayPermissions callback for when [WebsitePermissionsView] needs to be displayed.
 * @property engine An [Engine] instance used for clearing the browsing data.
 */
@Suppress("TooManyFunctions", "LongParameterList")
class DefaultQuickSettingsController(
    private val context: Context,
    private val quickSettingsStore: QuickSettingsFragmentStore,
    private val browserStore: BrowserStore,
    private val ioScope: CoroutineScope,
    private val navController: NavController,
    internal val sessionId: String,
    internal var sitePermissions: SitePermissions?,
    private val settings: Settings,
    private val permissionStorage: PermissionStorage,
    private val reload: ReloadUrlUseCase,
    private val requestRuntimePermissions: OnNeedToRequestPermissions = { },
    private val displayPermissions: () -> Unit,
    private val engine: Engine = context.components.core.engine,
) : QuickSettingsController {

    override fun handlePermissionsShown() {
        displayPermissions()
    }

    override fun handlePermissionToggled(permission: WebsitePermission) {
        val featureToggled = permission.phoneFeature

        when (permission.isBlockedByAndroid) {
            true -> handleAndroidPermissionRequest(featureToggled.androidPermissionsList)
            false -> {
                val permissions = sitePermissions
                if (permissions != null) {
                    val newPermissions = permissions.toggle(featureToggled)
                    handlePermissionsChange(newPermissions)
                    sitePermissions = newPermissions

                    quickSettingsStore.dispatch(
                        WebsitePermissionAction.TogglePermission(
                            featureToggled,
                            featureToggled.getActionLabel(context, newPermissions, settings),
                            featureToggled.shouldBeEnabled(context, newPermissions, settings),
                        ),
                    )
                } else {
                    navigateToManagePhoneFeature(featureToggled)
                }
            }
        }
    }

    override fun handleAndroidPermissionGranted(feature: PhoneFeature) {
        quickSettingsStore.dispatch(
            WebsitePermissionAction.TogglePermission(
                feature,
                feature.getActionLabel(context, sitePermissions, settings),
                feature.shouldBeEnabled(context, sitePermissions, settings),
            ),
        )
    }

    override fun handleAutoplayChanged(autoplayValue: AutoplayValue) {
        val permissions = sitePermissions

        sitePermissions = if (permissions == null) {
            val tab = browserStore.state.findTabOrCustomTab(sessionId)
            val origin = requireNotNull(tab?.content?.url?.getOrigin()) {
                "An origin is required to change a autoplay settings from the door hanger"
            }
            val sitePermissions =
                autoplayValue.createSitePermissionsFromCustomRules(origin, settings)
            handleAutoplayAdd(sitePermissions, tab?.content?.private ?: false)
            sitePermissions
        } else {
            val newPermission = autoplayValue.updateSitePermissions(permissions)
            handlePermissionsChange(autoplayValue.updateSitePermissions(newPermission))
            newPermission
        }
        quickSettingsStore.dispatch(
            WebsitePermissionAction.ChangeAutoplay(autoplayValue),
        )
    }

    override fun handleTrackingProtectionToggled(isEnabled: Boolean) {
        val components = context.components
        val sessionState = components.core.store.state.findTabOrCustomTab(sessionId)

        sessionState?.let { session ->
            val trackingProtectionUseCases = components.useCases.trackingProtectionUseCases
            val sessionUseCases = components.useCases.sessionUseCases

            if (isEnabled) {
                trackingProtectionUseCases.removeException(session.id)
            } else {
                TrackingProtection.exceptionAdded.record(NoExtras())
                trackingProtectionUseCases.addException(session.id)
            }

            sessionUseCases.reload.invoke(session.id)
        }

        quickSettingsStore.dispatch(
            TrackingProtectionAction.ToggleTrackingProtectionEnabled(
                isEnabled,
            ),
        )
    }

    override fun handleCookieBannerHandlingDetailsClicked() {
        CookieBanners.visitedPanel.record(NoExtras())

        navController.popBackStack()

        val state = quickSettingsStore.state.protectionsState
        val directions = NavGraphDirections
            .actionGlobalCookieBannerProtectionPanelDialogFragment(
                sessionId = sessionId,
                url = state.url,
                trackingProtectionEnabled = state.isTrackingProtectionEnabled,
                cookieBannerUIMode = state.cookieBannerUIMode,
                gravity = context.components.settings.toolbarPosition.androidGravity,
                sitePermissions = sitePermissions,
            )
        navController.navigate(directions)
    }

    override fun handleTrackingProtectionDetailsClicked() {
        navController.popBackStack()

        val state = quickSettingsStore.state.protectionsState
        val directions = NavGraphDirections
            .actionGlobalTrackingProtectionPanelDialogFragment(
                sessionId = sessionId,
                url = state.url,
                trackingProtectionEnabled = state.isTrackingProtectionEnabled,
                cookieBannerUIMode = state.cookieBannerUIMode,
                gravity = context.components.settings.toolbarPosition.androidGravity,
                sitePermissions = sitePermissions,
            )
        navController.navigate(directions)
    }

    override fun handleConnectionDetailsClicked() {
        navController.popBackStack()

        val state = quickSettingsStore.state.webInfoState
        val directions = ConnectionPanelDialogFragmentDirections
            .actionGlobalConnectionDetailsDialogFragment(
                sessionId = sessionId,
                title = state.websiteTitle,
                url = state.websiteUrl,
                isSecured = state.websiteSecurityUiValues == WebsiteSecurityUiValues.SECURE,
                certificateName = state.certificateName,
                gravity = context.components.settings.toolbarPosition.androidGravity,
                sitePermissions = sitePermissions,
            )
        navController.navigate(directions)
    }

    override fun handleClearSiteDataClicked(baseDomain: String) {
        engine.clearData(
            host = baseDomain,
            data = Engine.BrowsingData.select(
                Engine.BrowsingData.AUTH_SESSIONS,
                Engine.BrowsingData.ALL_SITE_DATA,
            ),
        )
    }

    /**
     * Request a certain set of runtime Android permissions.
     *
     * User's approval should be received in the [handleAndroidPermissionGranted] method but this is not enforced.
     *
     * @param requestedPermissions [Array]<[String]> runtime permissions needed to be requested.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun handleAndroidPermissionRequest(requestedPermissions: Array<String>) {
        requestRuntimePermissions(requestedPermissions)
    }

    /**
     * Updates the list of [SitePermissions] for this current website and reloads it to allow / block
     * new functionality in the web page.
     *
     * @param updatedPermissions [SitePermissions] updated website permissions.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun handlePermissionsChange(updatedPermissions: SitePermissions) {
        val tab = requireNotNull(browserStore.state.findTabOrCustomTab(sessionId)) {
            "A session is required to update permission"
        }
        ioScope.launch {
            permissionStorage.updateSitePermissions(updatedPermissions, tab.content.private)
            reload(sessionId)
        }
    }

    @VisibleForTesting
    internal fun handleAutoplayAdd(sitePermissions: SitePermissions, private: Boolean) {
        ioScope.launch {
            permissionStorage.add(sitePermissions, private)
            reload(sessionId)
        }
    }

    /**
     * Navigate to toggle [SitePermissions] for the specified [PhoneFeature]
     *
     * @param phoneFeature [PhoneFeature] to toggle [SitePermissions] for.
     */
    private fun navigateToManagePhoneFeature(phoneFeature: PhoneFeature) {
        val directions = QuickSettingsSheetDialogFragmentDirections
            .actionGlobalSitePermissionsManagePhoneFeature(phoneFeature)
        navController.navigate(directions)
    }
}
