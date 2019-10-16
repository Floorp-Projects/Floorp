/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.exceptions.ExceptionDomains
import org.mozilla.fenix.ext.tryGetHostFromUrl
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
     * Handles turning on/off tracking protection.
     *
     * @param websiteUrl [String] the website URL for which to toggle tracking protection.
     */
    fun handleTrackingProtectionToggled(websiteUrl: String, trackingEnabled: Boolean)

    /**
     * Handles showing the tracking protection settings.
     */
    fun handleTrackingProtectionSettingsSelected()

    /**
     * Handles reporting a webcompat issue for the indicated website.
     *
     * @param websiteUrl [String] the URL of the web page for which to report a site issue.
     */
    fun handleReportTrackingProblem(websiteUrl: String)

    /**
     * Handles the case of the [TrackingProtectionView] needed to be displayed to the user.
     */
    fun handleTrackingProtectionShown()

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
     * Handles a certain set of Android permissions being explicitly granted by the user.
     *
     * feature [PhoneFeature] which the user granted Android permission(s) for.
     */
    fun handleAndroidPermissionGranted(feature: PhoneFeature)
}

/**
 * Default behavior of [QuickSettingsController]. Other implementations are possible.
 *
 * @param context [Context] used for various Android interactions.
 * @param quickSettingsStore [QuickSettingsFragmentStore] holding the [State] for all Views displayed
 * in this Controller's Fragment.
 * @param coroutineScope [CoroutineScope] used for structed concurrency.
 * @param navController NavController] used for navigation.
 * @param session [Session]? current browser state.
 * @param sitePermissions [SitePermissions]? list of website permissions and their status.
 * @param settings [Settings] application settings.
 * @param permissionStorage [PermissionStorage] app state for website permissions exception.
 * @param trackingExceptions [ExceptionDomains] allows setting whether to allow trackers or not.
 * @param reload [ReloadUrlUseCase] callback allowing for reloading the current web page.
 * @param addNewTab [AddNewTabUseCase] callback allowing for loading a URL in a new tab.
 * @param requestRuntimePermissions [OnNeedToRequestPermissions] callback allowing for requesting
 * specific Android runtime permissions.
 * @param reportSiteIssue callback allowing to report an issue with the current web page.
 * @param displayTrackingProtection callback for when the [TrackingProtectionView] needs to be displayed.
 * @param displayPermissions callback for when [WebsitePermissionsView] needs to be displayed.
 * @param dismiss callback allowing to request this entire Fragment to be dismissed.
 */
@Suppress("TooManyFunctions")
class DefaultQuickSettingsController(
    private val context: Context,
    private val quickSettingsStore: QuickSettingsFragmentStore,
    private val coroutineScope: CoroutineScope,
    private val navController: NavController,
    private val session: Session?,
    private var sitePermissions: SitePermissions?,
    private val settings: Settings,
    private val permissionStorage: PermissionStorage,
    private val trackingExceptions: ExceptionDomains,
    private val reload: ReloadUrlUseCase,
    private val addNewTab: AddNewTabUseCase,
    private val requestRuntimePermissions: OnNeedToRequestPermissions = { },
    private val reportSiteIssue: () -> Unit,
    private val displayTrackingProtection: () -> Unit,
    private val displayPermissions: () -> Unit,
    private val dismiss: () -> Unit
) : QuickSettingsController {

    override fun handleTrackingProtectionToggled(
        websiteUrl: String,
        trackingEnabled: Boolean
    ) {
        val host = websiteUrl.tryGetHostFromUrl()
        trackingExceptions.toggle(host)
        reload(session)

        quickSettingsStore.dispatch(
            TrackingProtectionAction.TrackingProtectionToggled(trackingEnabled)
        )
    }

    override fun handleTrackingProtectionSettingsSelected() {
        val directions =
            QuickSettingsSheetDialogFragmentDirections
                .actionQuickSettingsSheetDialogFragmentToTrackingProtectionFragment()
        navController.navigate(directions)
    }

    @ExperimentalCoroutinesApi
    @UseExperimental(ObsoleteCoroutinesApi::class)
    override fun handleReportTrackingProblem(websiteUrl: String) {
        val reportUrl = String.format(BrowserFragment.REPORT_SITE_ISSUE_URL, websiteUrl)
        addNewTab(reportUrl)

        if (session?.isCustomTabSession() == true) {
            reportSiteIssue()
        }

        dismiss()
    }

    override fun handleTrackingProtectionShown() {
        displayTrackingProtection()
    }

    override fun handlePermissionsShown() {
        displayPermissions()
    }

    override fun handlePermissionToggled(permission: WebsitePermission) {
        val featureToggled = permission.getBackingFeature()

        when (permission.isBlockedByAndroid) {
            true -> handleAndroidPermissionRequest(featureToggled.androidPermissionsList)
            false -> {
                sitePermissions = sitePermissions!!.toggle(featureToggled).also {
                    handlePermissionsChange(it)
                }

                quickSettingsStore.dispatch(
                    WebsitePermissionAction.TogglePermission(
                        permission,
                        featureToggled.getActionLabel(context, sitePermissions, settings),
                        featureToggled.shouldBeEnabled(context, sitePermissions, settings)
                    )
                )
            }
        }
    }

    override fun handleAndroidPermissionGranted(feature: PhoneFeature) {
        quickSettingsStore.dispatch(
            WebsitePermissionAction.TogglePermission(
                feature.getCorrespondingPermission(),
                feature.getActionLabel(context, sitePermissions, settings),
                feature.shouldBeEnabled(context, sitePermissions, settings)
            )
        )
    }

    /**
     * Request a certain set of runtime Android permissions.
     *
     * User's approval should be received in the [handleAndroidPermissionGranted] method but this is not enforced.
     *
     * @param requestedPermissions [Array]<[String]> runtime permissions needed to be requested.
     */
    private fun handleAndroidPermissionRequest(requestedPermissions: Array<String>) {
        requestRuntimePermissions(requestedPermissions)
    }

    /**
     * Updates the list of [SitePermissions] for this current website and reloads it to allow / block
     * new functionality in the web page.
     *
     * @param updatedPermissions [SitePermissions] updated website permissions.
     */
    private fun handlePermissionsChange(updatedPermissions: SitePermissions) {
        coroutineScope.launch(Dispatchers.IO) {
            permissionStorage.updateSitePermissions(updatedPermissions)
            reload(session)
        }
    }

    /**
     * Each [WebsitePermission] is mapped after a [PhoneFeature].
     *
     * Get this [WebsitePermission]'s [PhoneFeature].
     */
    private fun WebsitePermission.getBackingFeature(): PhoneFeature = when (this) {
        is WebsitePermission.Camera -> PhoneFeature.CAMERA
        is WebsitePermission.Microphone -> PhoneFeature.MICROPHONE
        is WebsitePermission.Notification -> PhoneFeature.NOTIFICATION
        is WebsitePermission.Location -> PhoneFeature.LOCATION
    }

    /**
     * Get the specific [WebsitePermission] implementation which this [PhoneFeature] is tied to.
     *
     * **The result only informs about the type of [WebsitePermission].
     * The resulting object's properties are just stubs and not dependable.**
     */
    private fun PhoneFeature.getCorrespondingPermission(): WebsitePermission {
        val defaultStatus = ""
        val defaultEnabled = false
        val defaultVisible = false
        val defaultBlockedByAndroid = false
        val defaultWebsitePermission: WebsitePermission? = null

        return when (this) {
            PhoneFeature.CAMERA -> WebsitePermission.Camera(
                defaultStatus, defaultVisible, defaultEnabled, defaultBlockedByAndroid)
            PhoneFeature.LOCATION -> WebsitePermission.Location(
                defaultStatus, defaultVisible, defaultEnabled, defaultBlockedByAndroid)
            PhoneFeature.MICROPHONE -> WebsitePermission.Microphone(
                defaultStatus, defaultVisible, defaultEnabled, defaultBlockedByAndroid)
            PhoneFeature.NOTIFICATION -> WebsitePermission.Notification(
                defaultStatus, defaultVisible, defaultEnabled, defaultBlockedByAndroid)
            PhoneFeature.AUTOPLAY -> defaultWebsitePermission!! // fail-fast
        }
    }
}
