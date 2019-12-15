/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status
import mozilla.components.support.webextensions.WebExtensionSupport
import mozilla.components.support.webextensions.WebExtensionSupport.installedExtensions
import java.lang.IllegalStateException

/**
 * Provides access to installed and recommended [Addon]s and manages their states.
 *
 * @property store The application's [BrowserStore].
 * @property engine The application's browser [Engine].
 * @property addonsProvider The [AddonsProvider] to query available [Addon]s.
 * @property addonUpdater The [AddonUpdater] instance to use when checking / triggering
 * updates.
 */
class AddonManager(
    private val store: BrowserStore,
    private val engine: Engine,
    private val addonsProvider: AddonsProvider,
    private val addonUpdater: AddonUpdater
) {

    /**
     * Returns the list of all installed and recommended add-ons.
     *
     * @return list of all [Addon]s with up-to-date [Addon.installedState].
     * @throws AddonManagerException in case of a problem reading from
     * the [addonsProvider] or querying web extension state from the engine / store.
     */
    @Throws(AddonManagerException::class)
    @Suppress("TooGenericExceptionCaught")
    suspend fun getAddons(): List<Addon> {
        try {
            // Make sure extension support is initialized, i.e. the state of all installed extensions is known.
            WebExtensionSupport.awaitInitialization()

            return addonsProvider.getAvailableAddons().map { addon ->
                installedExtensions[addon.id]?.let { addon.copy(installedState = it.toInstalledState()) } ?: addon
            }
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
        engine.installWebExtension(
            id = addon.id,
            url = addon.downloadUrl,
            onSuccess = { ext ->
                val installedAddon = addon.copy(installedState = ext.toInstalledState())
                addonUpdater.registerForFutureUpdates(installedAddon.id)
                onSuccess(installedAddon)
            },
            onError = onError
        )
    }

    /**
     * Uninstalls the provided [Addon].
     *
     * @param addon the addon to uninstall.
     * @param onSuccess (optional) callback invoked if the addon was uninstalled successfully.
     * @param onError (optional) callback invoked if there was an error installing the addon.
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

        engine.uninstallWebExtension(
            extension,
            onSuccess = {
                addonUpdater.unregisterForFutureUpdates(addon.id)
                onSuccess()
            },
            onError = onError
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

        engine.enableWebExtension(
            extension,
            onSuccess = { ext ->
                val enabledAddon = addon.copy(installedState = ext.toInstalledState())
                onSuccess(enabledAddon)
            },
            onError = onError
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

        engine.disableWebExtension(
            extension,
            onSuccess = { ext ->
                val disabledAddon = addon.copy(installedState = ext.toInstalledState())
                onSuccess(disabledAddon)
            },
            onError = onError
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

        if (extension == null) {
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
}

/**
 * Wraps exceptions thrown by either the initialization process or an [AddonsProvider].
 */
class AddonManagerException(throwable: Throwable) : Exception(throwable)

private fun WebExtension.toInstalledState() =
    // TODO Add optionsUrl
    // TODO https://bugzilla.mozilla.org/show_bug.cgi?id=1598792
    // TODO https://bugzilla.mozilla.org/show_bug.cgi?id=1597793
    Addon.InstalledState(id, getMetadata()?.version ?: "", "https://mozilla.org", isEnabled())
