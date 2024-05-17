/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.Addon.Companion.isAllURLsPermission
import mozilla.components.feature.addons.ui.translateName
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.addons.ui.AddonPermissionsScreen
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Optional [Addon.Permission.name] lists for requesting permission updates for an [Addon].
 */
data class AddonPermissionsUpdateRequest(
    val optionalPermissions: List<String> = emptyList(),
    val originPermissions: List<String> = emptyList(),
)

/**
 * A fragment to show and allow a user to change permissions for an addon.
 */
class AddonPermissionsDetailsFragment : Fragment() {

    private val args by navArgs<AddonPermissionsDetailsFragmentArgs>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            val optionalPermissions = rememberSaveable {
                mutableStateOf(args.addon.translateOptionalPermissions(requireContext()))
            }

            val originPermissions = rememberSaveable {
                mutableStateOf(
                    args.addon.optionalOrigins
                        .map {
                            Addon.LocalizedPermission(it.name, it)
                        },
                )
            }

            // Note: Even if <all_urls> is in optionalPermissions of the extension manifest, it is found in
            // originPermissions of the Addon
            val allSitesHostPermissionsList = rememberSaveable {
                mutableStateOf(
                    args.addon.optionalOrigins.getAllSitesPermissionsList(),
                )
            }

            // Update all of the mutable states when an addon is returned from updating permissions
            val onUpdatePermissionsSuccess: (Addon) -> Unit = { updatedAddon ->
                optionalPermissions.value =
                    updatedAddon.translateOptionalPermissions(requireContext())
                originPermissions.value = updatedAddon.optionalOrigins.map {
                    Addon.LocalizedPermission(it.name, it)
                }
                allSitesHostPermissionsList.value =
                    updatedAddon.optionalOrigins.getAllSitesPermissionsList()
            }

            FirefoxTheme {
                AddonPermissionsScreen(
                    permissions = args.addon.translatePermissions(requireContext()),
                    optionalPermissions = optionalPermissions.value,
                    originPermissions = originPermissions.value,
                    isAllSitesSwitchVisible = allSitesHostPermissionsList.value.isNotEmpty(),
                    isAllSitesEnabled = allSitesHostPermissionsList.value.getOrNull(0)?.granted
                        ?: false,
                    onAddOptionalPermissions = { permissionRequest ->
                        addOptionalPermissions(permissionRequest, onUpdatePermissionsSuccess)
                    },
                    onRemoveOptionalPermissions = { permissionRequest ->
                        removeOptionalPermission(permissionRequest, onUpdatePermissionsSuccess)
                    },
                    onAddAllSitesPermissions = {
                        addOptionalPermissions(
                            AddonPermissionsUpdateRequest(
                                originPermissions = allSitesHostPermissionsList.value.map { it.name },
                            ),
                            onUpdatePermissionsSuccess,
                        )
                    },
                    onRemoveAllSitesPermissions = {
                        removeOptionalPermission(
                            AddonPermissionsUpdateRequest(
                                originPermissions = allSitesHostPermissionsList.value.map { it.name },
                            ),
                            onUpdatePermissionsSuccess,
                        )
                    },
                    onLearnMoreClick = { learnMoreUrl ->
                        openWebsite(learnMoreUrl)
                    },
                )
            }
        }
    }

    private fun addOptionalPermissions(
        addPermissionsRequest: AddonPermissionsUpdateRequest,
        onUpdatePermissionsSuccess: (Addon) -> Unit,
    ) {
        requireContext().components.addonManager.addOptionalPermission(
            args.addon,
            addPermissionsRequest.optionalPermissions,
            addPermissionsRequest.originPermissions,
            onSuccess = {
                onUpdatePermissionsSuccess(it)
            },
            onError = {
                /** No-Op **/
            },
        )
    }

    private fun removeOptionalPermission(
        removePermissionsRequest: AddonPermissionsUpdateRequest,
        onUpdatePermissionsSuccess: (Addon) -> Unit,
    ) {
        requireContext().components.addonManager.removeOptionalPermission(
            args.addon,
            removePermissionsRequest.optionalPermissions,
            removePermissionsRequest.originPermissions,
            onSuccess = {
                onUpdatePermissionsSuccess(it)
            },
            onError = {
                /** No-Op **/
            },
        )
    }

    private fun openWebsite(addonSiteUrl: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = addonSiteUrl,
            newTab = true,
            from = BrowserDirection.FromAddonPermissionsDetailsFragment,
        )
    }

    private fun <T : List<Addon.Permission>> T.getAllSitesPermissionsList(): List<Addon.Permission> {
        return this.mapNotNull {
            if (it.isAllURLsPermission()) {
                it
            } else {
                null
            }
        }
    }

    override fun onResume() {
        super.onResume()
        context?.let {
            showToolbar(title = args.addon.translateName(it))
        }
    }
}
