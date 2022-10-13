/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import android.os.Handler
import android.os.HandlerThread
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.android.asCoroutineDispatcher
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.CancellableOperation
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.concept.engine.webextension.isUnsupported
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status
import mozilla.components.support.webextensions.WebExtensionSupport
import mozilla.components.support.webextensions.WebExtensionSupport.installedExtensions
import java.util.Collections.newSetFromMap
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

/**
 * Provides access to installed and recommended [Addon]s and manages their states.
 *
 * @property store The application's [BrowserStore].
 * @property runtime The application's [WebExtensionRuntime] to install and configure extensions.
 * @property addonsProvider The [AddonsProvider] to query available [Addon]s.
 * @property addonUpdater The [AddonUpdater] instance to use when checking / triggering
 * updates.
 */
@Suppress("LargeClass")
class AddonManager(
    private val store: BrowserStore,
    private val runtime: WebExtensionRuntime,
    private val addonsProvider: AddonsProvider,
    private val addonUpdater: AddonUpdater,
) {

    @VisibleForTesting
    internal val pendingAddonActions = newSetFromMap(ConcurrentHashMap<CompletableDeferred<Unit>, Boolean>())

    /**
     * Returns the list of all installed and recommended add-ons.
     *
     * @param waitForPendingActions whether or not to wait (suspend, but not
     * block) until all pending add-on actions (install/uninstall/enable/disable)
     * are completed in either success or failure.
     * @param allowCache whether or not the result may be provided
     * from a previously cached response, defaults to true.
     * @return list of all [Addon]s with up-to-date [Addon.installedState].
     * @throws AddonManagerException in case of a problem reading from
     * the [addonsProvider] or querying web extension state from the engine / store.
     */
    @Throws(AddonManagerException::class)
    @Suppress("TooGenericExceptionCaught")
    suspend fun getAddons(waitForPendingActions: Boolean = true, allowCache: Boolean = true): List<Addon> {
        try {
            // Make sure extension support is initialized, i.e. the state of all installed extensions is known.
            WebExtensionSupport.awaitInitialization()

            // Make sure all pending actions are completed.
            if (waitForPendingActions) {
                pendingAddonActions.awaitAll()
            }

            // Get all available/supported addons from provider and add state if installed.
            // NB: We're keeping translations only for the default locale.
            val userLanguage = Locale.getDefault().language
            val locales = listOf(userLanguage)
            val supportedAddons = addonsProvider.getAvailableAddons(allowCache, language = userLanguage)
                .map {
                        addon ->
                    addon.filterTranslations(locales)
                }
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
                    val description = extension.getMetadata()?.description ?: extension.id

                    // Temporary add-ons should be treated as supported
                    val installedState = if (extension.getMetadata()?.temporary == true) {
                        val icon = withContext(getIconDispatcher()) {
                            extension.loadIcon(TEMPORARY_ADDON_ICON_SIZE)
                        }
                        extension.toInstalledState().copy(icon = icon)
                    } else {
                        extension.toInstalledState().copy(enabled = false, supported = false)
                    }

                    Addon(
                        id = extension.id,
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to name),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to description),
                        // We don't have a summary for unsupported add-ons, let's re-use description
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to description),
                        siteUrl = extension.url,
                        installedState = installedState,
                        updatedAt = "1970-01-01T00:00:00Z",
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
        onError: ((String, Throwable) -> Unit) = { _, _ -> },
    ): CancellableOperation {
        // Verify the add-on doesn't require blocked permissions
        // only available to built-in extensions
        BLOCKED_PERMISSIONS.forEach { blockedPermission ->
            if (addon.permissions.any { it.equals(blockedPermission, ignoreCase = true) }) {
                onError(addon.id, IllegalArgumentException("Addon requires invalid permission $blockedPermission"))
                return CancellableOperation.Noop()
            }
        }

        val pendingAction = addPendingAddonAction()
        return runtime.installWebExtension(
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
            },
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
        onError: ((String, Throwable) -> Unit) = { _, _ -> },
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(addon.id, IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        runtime.uninstallWebExtension(
            extension,
            onSuccess = {
                addonUpdater.unregisterForFutureUpdates(addon.id)
                completePendingAddonAction(pendingAction)
                onSuccess()
            },
            onError = { id, throwable ->
                completePendingAddonAction(pendingAction)
                onError(id, throwable)
            },
        )
    }

    /**
     * Enables the provided [Addon].
     *
     * @param addon the [Addon] to enable.
     * @param source [EnableSource] to indicate why the extension is enabled, default to EnableSource.USER.
     * @param onSuccess (optional) callback invoked with the enabled [Addon].
     * @param onError (optional) callback invoked if there was an error enabling
     */
    fun enableAddon(
        addon: Addon,
        source: EnableSource = EnableSource.USER,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { },
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        runtime.enableWebExtension(
            extension,
            source = source,
            onSuccess = { ext ->
                val enabledAddon = addon.copy(installedState = ext.toInstalledState())
                completePendingAddonAction(pendingAction)
                onSuccess(enabledAddon)
            },
            onError = {
                completePendingAddonAction(pendingAction)
                onError(it)
            },
        )
    }

    /**
     * Disables the provided [Addon].
     *
     * @param addon the [Addon] to disable.
     * @param source [EnableSource] to indicate why the addon is disabled, default to EnableSource.USER.
     * @param onSuccess (optional) callback invoked with the enabled [Addon].
     * @param onError (optional) callback invoked if there was an error enabling
     */
    fun disableAddon(
        addon: Addon,
        source: EnableSource = EnableSource.USER,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { },
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        runtime.disableWebExtension(
            extension,
            source,
            onSuccess = { ext ->
                val disabledAddon = addon.copy(installedState = ext.toInstalledState())
                completePendingAddonAction(pendingAction)
                onSuccess(disabledAddon)
            },
            onError = {
                completePendingAddonAction(pendingAction)
                onError(it)
            },
        )
    }

    /**
     * Sets whether to allow/disallow the provided [Addon] in private browsing mode.
     *
     * @param addon the [Addon] to allow/disallow.
     * @param allowed true if allow, false otherwise.
     * @param onSuccess (optional) callback invoked with the enabled [Addon].
     * @param onError (optional) callback invoked if there was an error enabling
     */
    fun setAddonAllowedInPrivateBrowsing(
        addon: Addon,
        allowed: Boolean,
        onSuccess: ((Addon) -> Unit) = { },
        onError: ((Throwable) -> Unit) = { },
    ) {
        val extension = addon.installedState?.let { installedExtensions[it.id] }
        if (extension == null) {
            onError(IllegalStateException("Addon is not installed"))
            return
        }

        val pendingAction = addPendingAddonAction()
        runtime.setAllowedInPrivateBrowsing(
            extension,
            allowed,
            onSuccess = { ext ->
                val modifiedAddon = addon.copy(installedState = ext.toInstalledState())
                completePendingAddonAction(pendingAction)
                onSuccess(modifiedAddon)
            },
            onError = {
                completePendingAddonAction(pendingAction)
                onError(it)
            },
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

        runtime.updateWebExtension(extension, onSuccess, onError)
    }

    private fun addPendingAddonAction() = CompletableDeferred<Unit>().also {
        pendingAddonActions.add(it)
    }

    private fun completePendingAddonAction(action: CompletableDeferred<Unit>) {
        action.complete(Unit)
        pendingAddonActions.remove(action)
    }

    @VisibleForTesting
    internal fun getIconDispatcher(): CoroutineDispatcher {
        val iconThread = HandlerThread("IconThread").also {
            it.start()
        }
        return Handler(iconThread.looper).asCoroutineDispatcher("WebExtensionIconDispatcher")
    }

    companion object {
        // List of invalid permissions for external add-ons i.e. permissions only
        // granted to built-in extensions:
        val BLOCKED_PERMISSIONS = listOf("geckoViewAddons", "nativeMessaging")

        // Size of the icon to load for temporary extensions
        const val TEMPORARY_ADDON_ICON_SIZE = 48
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
        disabledAsUnsupported = isUnsupported(),
        allowedInPrivateBrowsing = isAllowedInPrivateBrowsing(),
    )
