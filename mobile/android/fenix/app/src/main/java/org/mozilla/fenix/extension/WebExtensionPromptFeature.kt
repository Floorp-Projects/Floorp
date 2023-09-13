/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.extension

import android.content.Context
import android.view.Gravity
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.extension.WebExtensionPromptRequest
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionInstallException
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.toInstalledState
import mozilla.components.feature.addons.ui.AddonInstallationDialogFragment
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.appVersionName
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import java.lang.ref.WeakReference

/**
 * Feature implementation for handling [WebExtensionPromptRequest] and showing the respective UI.
 */
class WebExtensionPromptFeature(
    private val store: BrowserStore,
    private val context: Context,
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

                when (promptRequest) {
                    is WebExtensionPromptRequest.AfterInstallation -> {
                        handleAfterInstallationRequest(promptRequest)
                    }

                    is WebExtensionPromptRequest.BeforeInstallation.InstallationFailed -> {
                        handleBeforeInstallationRequest(promptRequest)
                        consumePromptRequest()
                    }
                }
            }
        }
        tryToReAttachButtonHandlersToPreviousDialog()
    }

    private fun handleAfterInstallationRequest(promptRequest: WebExtensionPromptRequest.AfterInstallation) {
        // The install flow in Fenix relies on an [Addon] object so let's convert the (GeckoView)
        // extension into a minimal add-on. The missing metadata will be fetched when the user
        // opens the add-ons manager.
        val addon = Addon.newFromWebExtension(promptRequest.extension)
        when (promptRequest) {
            is WebExtensionPromptRequest.AfterInstallation.Permissions -> handlePermissionRequest(
                addon,
                promptRequest,
            )
            is WebExtensionPromptRequest.AfterInstallation.PostInstallation -> handlePostInstallationRequest(
                addon.copy(installedState = promptRequest.extension.toInstalledState()),
            )
        }
    }

    private fun handleBeforeInstallationRequest(promptRequest: WebExtensionPromptRequest.BeforeInstallation) {
        when (promptRequest) {
            is WebExtensionPromptRequest.BeforeInstallation.InstallationFailed -> {
                handleInstallationFailedRequest(
                    exception = promptRequest.exception,
                )
                consumePromptRequest()
            }
        }
    }

    private fun handlePostInstallationRequest(
        addon: Addon,
    ) {
        showPostInstallationDialog(addon)
    }

    private fun handlePermissionRequest(
        addon: Addon,
        promptRequest: WebExtensionPromptRequest.AfterInstallation.Permissions,
    ) {
        if (hasExistingPermissionDialogFragment()) return
        showPermissionDialog(
            addon,
            promptRequest,
        )
    }

    @VisibleForTesting
    internal fun handleInstallationFailedRequest(
        exception: WebExtensionInstallException,
    ) {
        val addonName = exception.extensionName ?: ""
        var title = context.getString(R.string.mozac_feature_addons_failed_to_install, "")
        val message = when (exception) {
            is WebExtensionInstallException.Blocklisted -> {
                context.getString(R.string.mozac_feature_addons_blocklisted_1, addonName)
            }

            is WebExtensionInstallException.UserCancelled -> {
                // We don't want to show an error message when users cancel installation.
                return
            }

            is WebExtensionInstallException.Unknown -> {
                // Making sure we don't have a
                // Title = Failed to install
                // Message = Failed to install $addonName
                title = ""
                if (addonName.isNotEmpty()) {
                    context.getString(R.string.mozac_feature_addons_failed_to_install, addonName)
                } else {
                    context.getString(R.string.mozac_feature_addons_failed_to_install_generic)
                }
            }

            is WebExtensionInstallException.NetworkFailure -> {
                context.getString(R.string.mozac_feature_addons_failed_to_install_network_error)
            }

            is WebExtensionInstallException.CorruptFile -> {
                context.getString(R.string.mozac_feature_addons_failed_to_install_corrupt_error)
            }

            is WebExtensionInstallException.NotSigned -> {
                context.getString(
                    R.string.mozac_feature_addons_failed_to_install_not_signed_error,
                )
            }

            is WebExtensionInstallException.Incompatible -> {
                val appName = context.getString(R.string.app_name)
                val version = context.appVersionName
                context.getString(
                    R.string.mozac_feature_addons_failed_to_install_incompatible_error,
                    addonName,
                    appName,
                    version,
                )
            }
        }

        showDialog(
            title = title,
            message = message,
        )
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
        promptRequest: WebExtensionPromptRequest.AfterInstallation.Permissions,
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

    private fun tryToReAttachButtonHandlersToPreviousDialog() {
        findPreviousPermissionDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = { addon ->
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (promptRequest is WebExtensionPromptRequest.AfterInstallation.Permissions &&
                        addon.id == promptRequest.extension.id
                    ) {
                        handleApprovedPermissions(promptRequest)
                    }
                }
            }
            dialog.onNegativeButtonClicked = {
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (promptRequest is WebExtensionPromptRequest.AfterInstallation.Permissions) {
                        handleDeniedPermissions(promptRequest)
                    }
                }
            }
        }

        findPreviousPostInstallationDialogFragment()?.let { dialog ->
            dialog.onConfirmButtonClicked = { addon, allowInPrivateBrowsing ->
                store.state.webExtensionPromptRequest?.let { promptRequest ->
                    if (promptRequest is WebExtensionPromptRequest.AfterInstallation.PostInstallation &&
                        addon.id == promptRequest.extension.id
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

    private fun handleDeniedPermissions(promptRequest: WebExtensionPromptRequest.AfterInstallation.Permissions) {
        promptRequest.onConfirm(false)
        consumePromptRequest()
    }

    private fun handleApprovedPermissions(promptRequest: WebExtensionPromptRequest.AfterInstallation.Permissions) {
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

    @VisibleForTesting
    internal fun showDialog(
        title: String,
        message: String,
    ) {
        context.let {
            AlertDialog.Builder(it)
                .setTitle(title)
                .setPositiveButton(android.R.string.ok) { _, _ -> }
                .setCancelable(false)
                .setMessage(
                    message,
                )
                .show()
                .withCenterAlignedButtons()
        }
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val POST_INSTALLATION_DIALOG_FRAGMENT_TAG =
            "ADDONS_INSTALLATION_DIALOG_FRAGMENT"
    }
}
