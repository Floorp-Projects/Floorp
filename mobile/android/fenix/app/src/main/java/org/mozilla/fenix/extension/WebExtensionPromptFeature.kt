/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.extension

import android.content.Context
import android.view.Gravity
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.extension.WebExtensionPromptRequest
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.toInstalledState
import mozilla.components.feature.addons.ui.AddonInstallationDialogFragment
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import java.lang.ref.WeakReference

/**
 * Feature implementation for handling [WebExtensionPromptRequest] and showing the respective UI.
 */
class WebExtensionPromptFeature(
    private val store: BrowserStore,
    private val provideAddons: suspend () -> List<Addon>,
    private val context: Context,
    private val view: View,
    private val fragmentManager: FragmentManager,
    private val onAddonChanged: (Addon) -> Unit = {},
) : LifecycleAwareFeature {

    /**
     * Whether or not an add-on installation is in progress.
     */
    private var isInstallationInProgress = false
    private var scope: CoroutineScope? = null

    /**
     * Starts observing the selected session to listen for window requests
     * and opens / closes tabs as needed.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state ->
                state.webExtensionPromptRequest
            }.distinctUntilChanged().collect { promptRequest ->
                val addon = provideAddons().find { addon ->
                    addon.id == promptRequest.extension.id
                }
                when (promptRequest) {
                    is WebExtensionPromptRequest.Permissions -> handlePermissionRequest(
                        addon,
                        promptRequest,
                    )

                    is WebExtensionPromptRequest.PostInstallation -> handlePostInstallationRequest(
                        addon?.copy(installedState = promptRequest.extension.toInstalledState()),
                    )
                }
            }
        }
        tryToReAttachButtonHandlersToPreviousDialog()
    }

    private fun handlePostInstallationRequest(
        addon: Addon?,
    ) {
        if (addon == null) {
            consumePromptRequest()
            return
        }
        showPostInstallationDialog(addon)
    }

    private fun handlePermissionRequest(
        addon: Addon?,
        promptRequest: WebExtensionPromptRequest.Permissions,
    ) {
        if (hasExistingPermissionDialogFragment()) return

        if (addon == null) {
            promptRequest.onConfirm(false)
            consumePromptRequest()
            showUnsupportedError()
        } else {
            showPermissionDialog(
                addon,
                promptRequest,
            )
        }
    }

    /**
     * Stops observing the selected session for incoming window requests.
     */
    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting
    internal fun showPermissionDialog(
        addon: Addon,
        promptRequest: WebExtensionPromptRequest.Permissions,
    ) {
        if (!isInstallationInProgress && !hasExistingPermissionDialogFragment()) {
            val dialog = PermissionsDialogFragment.newInstance(
                addon = addon,
                promptsStyling = PermissionsDialogFragment.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        context,
                    ),
                    positiveButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.textOnColorPrimary,
                        context,
                    ),
                    positiveButtonRadius =
                    (context.resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat(),
                ),
                onPositiveButtonClicked = {
                    handleApprovedPermissions(promptRequest)
                },
                onNegativeButtonClicked = {
                    handleDeniedPermissions(promptRequest)
                },
            )
            dialog.show(
                fragmentManager,
                PERMISSIONS_DIALOG_FRAGMENT_TAG,
            )
        }
    }

    @VisibleForTesting
    internal fun showUnsupportedError() {
        showSnackBar(
            view,
            context.getString(R.string.addon_not_supported_error),
            FenixSnackbar.LENGTH_LONG,
        )
    }

    private fun tryToReAttachButtonHandlersToPreviousDialog() {
        findPreviousPermissionDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = { addon ->
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (addon.id == promptRequest.extension.id &&
                        promptRequest is WebExtensionPromptRequest.Permissions
                    ) {
                        handleApprovedPermissions(promptRequest)
                    }
                }
            }
            dialog.onNegativeButtonClicked = {
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (promptRequest is WebExtensionPromptRequest.Permissions) {
                        handleDeniedPermissions(promptRequest)
                    }
                }
            }
        }

        findPreviousPostInstallationDialogFragment()?.let { dialog ->
            dialog.onConfirmButtonClicked = { addon, allowInPrivateBrowsing ->
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (addon.id == promptRequest.extension.id &&
                        promptRequest is WebExtensionPromptRequest.PostInstallation
                    ) {
                        handlePostInstallationButtonClicked(
                            allowInPrivateBrowsing = allowInPrivateBrowsing,
                            context = WeakReference(context),
                            addon = addon,
                        )
                    }
                }
            }
            dialog.onDismissed = {
                store.state.webExtensionPromptRequest?.let { _ ->
                    consumePromptRequest()
                }
            }
        }
    }

    private fun handleDeniedPermissions(promptRequest: WebExtensionPromptRequest.Permissions) {
        promptRequest.onConfirm(false)
        consumePromptRequest()
    }

    private fun handleApprovedPermissions(promptRequest: WebExtensionPromptRequest.Permissions) {
        promptRequest.onConfirm(true)
        consumePromptRequest()
    }

    @VisibleForTesting
    internal fun consumePromptRequest() {
        store.dispatch(WebExtensionAction.ConsumePromptRequestWebExtensionAction)
    }

    private fun hasExistingPermissionDialogFragment(): Boolean {
        return findPreviousPermissionDialogFragment() != null
    }

    private fun hasExistingAddonPostInstallationDialogFragment(): Boolean {
        return fragmentManager.findFragmentByTag(POST_INSTALLATION_DIALOG_FRAGMENT_TAG)
            as? AddonInstallationDialogFragment != null
    }

    private fun findPreviousPermissionDialogFragment(): PermissionsDialogFragment? {
        return fragmentManager.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun findPreviousPostInstallationDialogFragment(): AddonInstallationDialogFragment? {
        return fragmentManager.findFragmentByTag(
            POST_INSTALLATION_DIALOG_FRAGMENT_TAG,
        ) as? AddonInstallationDialogFragment
    }

    private fun showPostInstallationDialog(addon: Addon) {
        if (!isInstallationInProgress && !hasExistingAddonPostInstallationDialogFragment()) {
            val addonsProvider = context.components.addonsProvider

            // Fragment may not be attached to the context anymore during onConfirmButtonClicked handling,
            // but we still want to be able to process user selection of the 'allowInPrivateBrowsing' pref.
            // This is a best-effort attempt to do so - retain a weak reference to the application context
            // (to avoid a leak), which we attempt to use to access addonManager.
            // See https://github.com/mozilla-mobile/fenix/issues/15816
            val weakApplicationContext: WeakReference<Context> = WeakReference(context)

            val dialog = AddonInstallationDialogFragment.newInstance(
                addon = addon,
                addonsProvider = addonsProvider,
                promptsStyling = AddonInstallationDialogFragment.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    confirmButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        context,
                    ),
                    confirmButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.textOnColorPrimary,
                        context,
                    ),
                    confirmButtonRadius =
                    (context.resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat(),
                ),
                onDismissed = {
                    consumePromptRequest()
                },
                onConfirmButtonClicked = { _, allowInPrivateBrowsing ->
                    handlePostInstallationButtonClicked(
                        addon = addon,
                        context = weakApplicationContext,
                        allowInPrivateBrowsing = allowInPrivateBrowsing,
                    )
                },
            )
            dialog.show(fragmentManager, POST_INSTALLATION_DIALOG_FRAGMENT_TAG)
        }
    }

    private fun handlePostInstallationButtonClicked(
        context: WeakReference<Context>,
        allowInPrivateBrowsing: Boolean,
        addon: Addon,
    ) {
        if (allowInPrivateBrowsing) {
            context.get()?.components?.addonManager?.setAddonAllowedInPrivateBrowsing(
                addon = addon,
                allowed = true,
                onSuccess = { updatedAddon ->
                    onAddonChanged(updatedAddon)
                },
            )
        }
        consumePromptRequest()
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val POST_INSTALLATION_DIALOG_FRAGMENT_TAG =
            "ADDONS_INSTALLATION_DIALOG_FRAGMENT"
    }
}
