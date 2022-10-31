/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.Service
import android.net.Uri
import android.os.Binder
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.browser.customtabs.CustomTabsService
import androidx.browser.customtabs.CustomTabsSessionToken
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.customtabs.feature.OriginVerifierFeature
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.customtabs.store.SaveCreatorPackageNameAction
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.support.base.log.logger.Logger

/**
 * Maximum number of speculative connections we will open when an app calls into
 * [AbstractCustomTabsService.mayLaunchUrl] with a list of URLs.
 */
private const val MAX_SPECULATIVE_URLS = 50

/**
 * [Service] providing Custom Tabs related functionality.
 */
abstract class AbstractCustomTabsService : CustomTabsService() {
    private val logger = Logger("CustomTabsService")
    private val scope = MainScope()

    abstract val engine: Engine
    abstract val customTabsServiceStore: CustomTabsServiceStore
    open val relationChecker: RelationChecker? = null

    @VisibleForTesting
    internal val verifier by lazy {
        relationChecker?.let { checker ->
            OriginVerifierFeature(packageManager, checker) { customTabsServiceStore.dispatch(it) }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
    }

    override fun warmup(flags: Long): Boolean {
        // We need to run this on the main thread since that's where GeckoRuntime expects to get initialized (if needed)
        return runBlocking(Main) {
            engine.warmUp()
            true
        }
    }

    override fun requestPostMessageChannel(sessionToken: CustomTabsSessionToken, postMessageOrigin: Uri): Boolean {
        return false
    }

    /**
     * Saves the package name of the app creating the custom tab when a new session is started.
     */
    override fun newSession(sessionToken: CustomTabsSessionToken): Boolean {
        // Extract the process UID of the app creating the custom tab.
        val uid = Binder.getCallingUid()
        // Only save the package if exactly one package name maps to the process UID.
        val packageName = packageManager.getPackagesForUid(uid)?.singleOrNull()

        if (!packageName.isNullOrEmpty()) {
            customTabsServiceStore.dispatch(SaveCreatorPackageNameAction(sessionToken, packageName))
        }
        return true
    }

    override fun extraCommand(commandName: String, args: Bundle?): Bundle? = null

    override fun mayLaunchUrl(
        sessionToken: CustomTabsSessionToken,
        url: Uri?,
        extras: Bundle?,
        otherLikelyBundles: List<Bundle>?,
    ): Boolean {
        logger.debug("Opening speculative connections")

        // Most likely URL for a future navigation: Open a speculative connection.
        url?.let { engine.speculativeConnect(it.toString()) }

        // A list of other likely URLs. Let's open a speculative connection for them up to a limit.
        otherLikelyBundles?.take(MAX_SPECULATIVE_URLS)?.forEach { bundle ->
            bundle.getParcelable<Uri>(KEY_URL)?.let { uri ->
                engine.speculativeConnect(uri.toString())
            }
        }

        return true
    }

    override fun postMessage(sessionToken: CustomTabsSessionToken, message: String, extras: Bundle?) =
        RESULT_FAILURE_DISALLOWED

    override fun validateRelationship(
        sessionToken: CustomTabsSessionToken,
        @Relation relation: Int,
        origin: Uri,
        extras: Bundle?,
    ): Boolean {
        val verifier = verifier
        val state = customTabsServiceStore.state.tabs[sessionToken]
        return if (verifier != null && state != null) {
            scope.launch(Main) {
                val result = verifier.verify(state, sessionToken, relation, origin)
                sessionToken.callback?.onRelationshipValidationResult(relation, origin, result, extras)
            }
            true
        } else {
            false
        }
    }

    override fun updateVisuals(sessionToken: CustomTabsSessionToken, bundle: Bundle?): Boolean {
        return false
    }

    override fun receiveFile(sessionToken: CustomTabsSessionToken, uri: Uri, purpose: Int, extras: Bundle?): Boolean {
        return false
    }
}
