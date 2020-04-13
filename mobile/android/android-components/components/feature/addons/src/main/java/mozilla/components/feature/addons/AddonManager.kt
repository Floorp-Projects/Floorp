/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.awaitAll
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.isUnsupported
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status
import mozilla.components.support.webextensions.WebExtensionSupport
import mozilla.components.support.webextensions.WebExtensionSupport.installedExtensions
import java.lang.IllegalArgumentException
import java.lang.IllegalStateException
import java.util.Collections.newSetFromMap
import java.util.concurrent.ConcurrentHashMap
import java.util.Locale

/**
 * Provides access to installed and recommended [Addon]s and manages their states.
 *
 * @property store The application's [BrowserStore].
 * @property engine The application's browser [Engine].
 * @property addonsProvider The [AddonsProvider] to query available [Addon]s.
 * @property addonUpdater The [AddonUpdater] instance to use when checking / triggering
 * updates.
 */
@Suppress("LargeClass")
class AddonManager(
    private val store: BrowserStore,
    private val engine: Engine,
    private val addonsProvider: AddonsProvider,
    private val addonUpdater: AddonUpdater
) {

    @VisibleForTesting
    internal val pendingAddonActions = newSetFromMap(ConcurrentHashMap<CompletableDeferred<Unit>, Boolean>())

    /**
     * Returns the list of all installed and recommended add-ons.
     *
     * @param waitForPendingActions whether or not to wait (suspend, but not
     * block) until all pending add-on actions (install/uninstall/enable/disable)
     * are completed in either success or failure.
     * @return list of all [Addon]s with up-to-date [Addon.installedState].
     * @throws AddonManagerException in case of a problem reading from
     * the [addonsProvider] or querying web extension state from the engine / store.
     */
    @Throws(AddonManagerException::class)
    @Suppress("TooGenericExceptionCaught")
    suspend fun getAddons(waitForPendingActions: Boolean = true): List<Addon> {
        try {
            // Make sure extension support is initialized, i.e. the state of all installed extensions is known.
            WebExtensionSupport.awaitInitialization()

            // Make sure all pending actions are completed
            if (waitForPendingActions) {
                pendingAddonActions.awaitAll()
            }

            // Get all available/supported addons from provider and add state if
            // installed. NB: We're keeping only the translations of the default
            val locales = listOf(Locale.getDefault().language)
            val supportedAddons = addonsProvider.getAvailableAddons()
                .map { addon -> addon.filterTranslations(locales) }
                .map { addon ->
                    installedExtensions[addon.id]?.let {
                        addon.copy(installedState = it.toInstalledState())
                    } ?: addon
                }

            val supportedAddonIds = supportedAddons.map { it.id }

            // Get all installed addons that are not yet supported.
            val unsupportedAddons = installedExtensions
                .filterKeys { !supportedAddonIds.contains(it) }
                .filterValues { !it.isBuiltIn() }
                .map { extensionEntry ->
                    val extension: WebExtension = extensionEntry.value
                    val name = extension.getMetadata()?.name ?: extension.id
                    val installedState =
                        extension.toInstalledState().copy(enabled = false, supported = false)
                    Addon(
                        id = extension.id,
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to name),
                        siteUrl = extension.url,
                        installedState = installedState
                    )
                }

            return supportedAddons + unsupportedAddons
        } catch (throwable: Throwable) {
            throw AddonManagerException(throwable)
        }
    }

    /**
     * Installs the provided [Addon].
     *
     * @param addon the addon to install.
     * @param onSuccess (optional) callback invoked if the addon was installed successfully,
     * providing access to the [Addon] object.
     * @param onError (optional) callback invoked if there was an error installing the addon.
     */
    fun installAddon(
        addon: Addon,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((String, Throwable) -> Unit) = { _, _ -> }
    ) {

        // Verify the add-on doesn't require blocked permissions
        // only available to built-in extensions
        BLOCKED_PERMISSIONS.forEach { blockedPermission ->
            if (addon.permissions.any { it.equals(blockedPermission, ignoreCase = true) }) {
                onError(addon.id, IllegalArgumentException("Addon requires invalid permission $blockedPermission"))
                return
            }
        }

        val pendingAction = addPendingAddonAction()
        engine.installWebExtension(
            id = addon.id,
            url = addon.downloadUrl,
            onSuccess = { ext ->
                val installedAddon = addon.copy(installedState = ext.toInstalledState())
                addonUpdater.registerForFutureUpdates(installedAddon.id)
                completePendingAddonAction(pendingAction)
                onSuccess(installedAddon)
            },
            onError = { id, throwable ->
                completePendingAddonAction(pendingAction)
                onError(id, throwable)
            }
        )
    }

    /**
     * Uninstalls the provided [Addon].
     *
     * @param addon the addon to uninstall.
     * @param onSuccess (optional) callback invoked if the addon was uninstalled successfully.
     * @param onError (optional) callback invoked if there was an error uninstalling the addon.
     */
    fun uninstallAddon(
        addon: Addon,
        onSuccess: (() -> Unit) = { },
        onError: ((String, Throwable) -> Unit) = { _, _ -> }
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(addon.id, IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        engine.uninstallWebExtension(
            extension,
            onSuccess = {
                addonUpdater.unregisterForFutureUpdates(addon.id)
                completePendingAddonAction(pendingAction)
                onSuccess()
            },
            onError = { id, throwable ->
                completePendingAddonAction(pendingAction)
                onError(id, throwable)
            }
        )
    }

    /**
     * Enables the provided [Addon].
     *
     * @param addon the [Addon] to enable.
     * @param onSuccess (optional) callback invoked with the enabled [Addon].
     * @param onError (optional) callback invoked if there was an error enabling
     */
    fun enableAddon(
        addon: Addon,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { }
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        engine.enableWebExtension(
            extension,
            onSuccess = { ext ->
                val enabledAddon = addon.copy(installedState = ext.toInstalledState())
                completePendingAddonAction(pendingAction)
                onSuccess(enabledAddon)
            },
            onError = {
                completePendingAddonAction(pendingAction)
                onError(it)
            }
        )
    }

    /**
     * Disables the provided [Addon].
     *
     * @param addon the [Addon] to disable.
     * @param onSuccess (optional) callback invoked with the enabled [Addon].
     * @param onError (optional) callback invoked if there was an error enabling
     */
    fun disableAddon(
        addon: Addon,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { }
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        engine.disableWebExtension(
            extension,
            onSuccess = { ext ->
                val disabledAddon = addon.copy(installedState = ext.toInstalledState())
                completePendingAddonAction(pendingAction)
                onSuccess(disabledAddon)
            },
            onError = {
                completePendingAddonAction(pendingAction)
                onError(it)
            }
        )
    }

    /**
     * Updates the addon with the provided [id] if an update is available.
     *
     * @param id the ID of the addon
     * @param onFinish callback invoked with the [Status] of the update once complete.
     */
    fun updateAddon(id: String, onFinish: ((Status) -> Unit)) {
        val extension = installedExtensions[id]

        if (extension == null || extension.isUnsupported()) {
            onFinish(Status.NotInstalled)
            return
        }

        val onSuccess: ((WebExtension?) -> Unit) = { updatedExtension ->
            val status = if (updatedExtension == null) {
                Status.NoUpdateAvailable
            } else {
                WebExtensionSupport.markExtensionAsUpdated(store, updatedExtension)
                Status.SuccessfullyUpdated
            }
            onFinish(status)
        }

        val onError: ((String, Throwable) -> Unit) = { message, exception ->
            onFinish(Status.Error(message, exception))
        }

        engine.updateWebExtension(extension, onSuccess, onError)
    }

    private fun addPendingAddonAction() = CompletableDeferred<Unit>().also {
        pendingAddonActions.add(it)
    }

    private fun completePendingAddonAction(action: CompletableDeferred<Unit>) {
        action.complete(Unit)
        pendingAddonActions.remove(action)
    }

    companion object {
        // List of invalid permissions for external add-ons i.e. permissions only
        // granted to built-in extensions:
        val BLOCKED_PERMISSIONS = listOf("geckoViewAddons", "nativeMessaging")
    }
}

/**
 * Wraps exceptions thrown by either the initialization process or an [AddonsProvider].
 */
class AddonManagerException(throwable: Throwable) : Exception(throwable)

private fun WebExtension.toInstalledState() =
    Addon.InstalledState(
        id = id,
        version = getMetadata()?.version ?: "",
        optionsPageUrl = getMetadata()?.optionsPageUrl,
        openOptionsPageInTab = getMetadata()?.openOptionsPageInTab ?: false,
        enabled = isEnabled(),
        disabledAsUnsupported = isUnsupported()
    )
