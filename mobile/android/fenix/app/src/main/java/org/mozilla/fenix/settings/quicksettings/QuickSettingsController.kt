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
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.SessionUseCases.ReloadUrlUseCase
import mozilla.components.feature.tabs.TabsUseCases.AddNewTabUseCase
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.ktx.kotlin.getOrigin
import org.mozilla.fenix.R
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
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
     * @see [TrackingProtectionInteractor.onTrackingProtectionToggled]
     */
    fun handleTrackingProtectionToggled(isEnabled: Boolean)

    /**
     * @see [TrackingProtectionInteractor.onBlockedItemsClicked]
     */
    fun handleBlockedItemsClicked()
}

/**
 * Default behavior of [QuickSettingsController]. Other implementations are possible.
 *
 * @param context [Context] used for various Android interactions.
 * @param quickSettingsStore [QuickSettingsFragmentStore] holding the State for all Views displayed
 * in this Controller's Fragment.
 * @param ioScope [CoroutineScope] with an IO dispatcher used for structured concurrency.
 * @param navController NavController] used for navigation.
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
    private val browserStore: BrowserStore,
    private val ioScope: CoroutineScope,
    private val navController: NavController,
    @VisibleForTesting
    internal val sessionId: String,
    @VisibleForTesting
    internal var sitePermissions: SitePermissions?,
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

    override fun handleAutoplayChanged(autoplayValue: AutoplayValue) {
        val permissions = sitePermissions

        sitePermissions = if (permissions == null) {
            val tab = browserStore.state.findTabOrCustomTab(sessionId)
            val origin = requireNotNull(tab?.content?.url?.getOrigin()) {
                "An origin is required to change a autoplay settings from the door hanger"
            }
            val sitePermissions =
                autoplayValue.createSitePermissionsFromCustomRules(origin, settings)
            handleAutoplayAdd(sitePermissions)
            sitePermissions
        } else {
            val newPermission = autoplayValue.updateSitePermissions(permissions)
            handlePermissionsChange(autoplayValue.updateSitePermissions(newPermission))
            newPermission
        }
        quickSettingsStore.dispatch(
            WebsitePermissionAction.ChangeAutoplay(autoplayValue)
        )
    }

    override fun handleTrackingProtectionToggled(isEnabled: Boolean) {
        TODO("Not yet implemented")
    }

    override fun handleBlockedItemsClicked() {
        dismiss.invoke()

        val state = quickSettingsStore.state.trackingProtectionState
        val directions = QuickSettingsSheetDialogFragmentDirections
            .actionGlobalTrackingProtectionPanelDialogFragment(
                sessionId = sessionId,
                url = state.url,
                trackingProtectionEnabled = state.isTrackingProtectionEnabled,
                gravity = context.components.settings.toolbarPosition.androidGravity
            )
        navController.nav(R.id.quickSettingsSheetDialogFragment, directions)
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
        ioScope.launch {
            permissionStorage.updateSitePermissions(updatedPermissions)
            reload(sessionId)
        }
    }

    @VisibleForTesting
    internal fun handleAutoplayAdd(sitePermissions: SitePermissions) {
        ioScope.launch {
            permissionStorage.add(sitePermissions)
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
