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

interface QuickSettingsController {
    fun handleTrackingProtectionToggled(websiteUrl: String, trackingEnabled: Boolean)
    fun handleTrackingProtectionSettingsSelected()
    fun handleReportTrackingProblem(websiteUrl: String)
    fun handleTrackingProtectionShown()
    fun handlePermissionsShown()
    fun handlePermissionToggled(permission: WebsitePermission)
    fun handleAndroidPermissionGranted(feature: PhoneFeature)
}

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

    private fun handleAndroidPermissionRequest(requestedPermissions: Array<String>) {
        requestRuntimePermissions(requestedPermissions)
    }

    private fun handlePermissionsChange(updatedPermissions: SitePermissions) {
        coroutineScope.launch(Dispatchers.IO) {
            permissionStorage.updateSitePermissions(updatedPermissions)
            reload(session)
        }
    }

    private fun WebsitePermission.getBackingFeature(): PhoneFeature = when (this) {
        is WebsitePermission.Camera -> PhoneFeature.CAMERA
        is WebsitePermission.Microphone -> PhoneFeature.MICROPHONE
        is WebsitePermission.Notification -> PhoneFeature.NOTIFICATION
        is WebsitePermission.Location -> PhoneFeature.LOCATION
    }

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
