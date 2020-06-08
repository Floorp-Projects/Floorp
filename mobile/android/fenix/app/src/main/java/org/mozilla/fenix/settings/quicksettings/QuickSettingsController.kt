/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import org.mozilla.fenix.components.PermissionStorage
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
 * @param quickSettingsStore [QuickSettingsFragmentStore] holding the State for all Views displayed
 * in this Controller's Fragment.
 * @param coroutineScope [CoroutineScope] used for structed concurrency.
 * @param navController NavController] used for navigation.
 * @param session [Session]? current browser state.
 * @param sitePermissions [SitePermissions]? list of website permissions and their status.
 * @param settings [Settings] application settings.
 * @param permissionStorage [PermissionStorage] app state for website permissions exception.
 * @param reload [ReloadUrlUseCase] callback allowing for reloading the current web page.
 * @param addNewTab [AddNewTabUseCase] callback allowing for loading a URL in a new tab.
 * @param requestRuntimePermissions [OnNeedToRequestPermissions] callback allowing for requesting
 * specific Android runtime permissions.
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
    private val reload: ReloadUrlUseCase,
    private val addNewTab: AddNewTabUseCase,
    private val requestRuntimePermissions: OnNeedToRequestPermissions = { },
    private val displayPermissions: () -> Unit,
    private val dismiss: () -> Unit
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
                            featureToggled.shouldBeEnabled(context, newPermissions, settings)
                        )
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
        coroutineScope.launch(Dispatchers.IO) {
            permissionStorage.updateSitePermissions(updatedPermissions)
            reload(session)
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
