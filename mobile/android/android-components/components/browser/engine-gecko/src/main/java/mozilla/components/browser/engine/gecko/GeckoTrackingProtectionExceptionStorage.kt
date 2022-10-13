/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.engine.gecko.content.blocking.GeckoTrackingProtectionException
import mozilla.components.browser.engine.gecko.ext.geckoTrackingProtectionPermission
import mozilla.components.browser.engine.gecko.ext.isExcludedForTrackingProtection
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.components.support.ktx.kotlin.stripDefaultPort
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY

/**
 * A [TrackingProtectionExceptionStorage] implementation to store tracking protection exceptions.
 */
internal class GeckoTrackingProtectionExceptionStorage(
    private val runtime: GeckoRuntime,
) : TrackingProtectionExceptionStorage {
    internal var scope = CoroutineScope(Dispatchers.IO)

    override fun contains(session: EngineSession, onResult: (Boolean) -> Unit) {
        val url = (session as GeckoEngineSession).currentUrl
        if (!url.isNullOrEmpty()) {
            getPermissions(url) { permissions ->
                val contains = permissions.isNotEmpty()
                onResult(contains)
            }
        } else {
            onResult(false)
        }
    }

    override fun fetchAll(onResult: (List<TrackingProtectionException>) -> Unit) {
        runtime.storageController.allPermissions.accept { permissions ->
            val trackingExceptions = permissions.filterTrackingProtectionExceptions()
            onResult(trackingExceptions.map { exceptions -> exceptions.toTrackingProtectionException() })
        }
    }

    private fun List<ContentPermission>?.filterTrackingProtectionExceptions() =
        this.orEmpty().filter { it.isExcludedForTrackingProtection }

    private fun List<ContentPermission>?.filterTrackingProtectionExceptions(url: String) =
        this.orEmpty()
            .filter {
                it.isExcludedForTrackingProtection && it.uri.getOrigin().orEmpty()
                    .stripDefaultPort() == url
            }

    override fun add(session: EngineSession, persistInPrivateMode: Boolean) {
        val geckoEngineSession = (session as GeckoEngineSession)
        if (persistInPrivateMode) {
            addPersistentPrivateException(geckoEngineSession)
        } else {
            geckoEngineSession.geckoTrackingProtectionPermission?.let {
                runtime.storageController.setPermission(it, VALUE_ALLOW)
            }
        }

        geckoEngineSession.notifyObservers {
            onExcludedOnTrackingProtectionChange(true)
        }
    }

    internal fun addPersistentPrivateException(geckoEngineSession: GeckoEngineSession) {
        val permission = geckoEngineSession.geckoTrackingProtectionPermission
        permission?.let {
            runtime.storageController.setPrivateBrowsingPermanentPermission(it, VALUE_ALLOW)
        }
    }

    override fun remove(session: EngineSession) {
        val geckoEngineSession = (session as GeckoEngineSession)
        val url = geckoEngineSession.currentUrl ?: return
        geckoEngineSession.notifyObservers {
            onExcludedOnTrackingProtectionChange(false)
        }
        remove(url)
    }

    override fun remove(exception: TrackingProtectionException) {
        if (exception is GeckoTrackingProtectionException) {
            remove(exception.contentPermission)
        } else {
            remove(exception.url)
        }
    }

    @VisibleForTesting
    internal fun remove(url: String) {
        val storage = runtime.storageController
        getPermissions(url) { permissions ->
            permissions.forEach { geckoPermissions ->
                storage.setPermission(geckoPermissions, VALUE_DENY)
            }
        }
    }

    // This is a workaround until https://bugzilla.mozilla.org/show_bug.cgi?id=1723280 gets addressed
    private fun getPermissions(url: String, onFinish: (List<ContentPermission>) -> Unit) {
        val localUrl = url.getOrigin().orEmpty().stripDefaultPort()
        val storage = runtime.storageController
        if (localUrl.isNotEmpty()) {
            storage.allPermissions.accept { permissions ->
                onFinish(permissions.filterTrackingProtectionExceptions(localUrl))
            }
        } else {
            onFinish(emptyList())
        }
    }

    @VisibleForTesting
    internal fun remove(contentPermission: ContentPermission) {
        runtime.storageController.setPermission(contentPermission, VALUE_DENY)
    }

    override fun removeAll(activeSessions: List<EngineSession>?, onRemove: () -> Unit) {
        val storage = runtime.storageController
        activeSessions?.forEach { engineSession ->
            engineSession.notifyObservers {
                onExcludedOnTrackingProtectionChange(false)
            }
        }
        storage.allPermissions.accept { permissions ->
            val trackingExceptions = permissions.filterTrackingProtectionExceptions()
            trackingExceptions.forEach {
                storage.setPermission(it, VALUE_DENY)
            }
            onRemove.invoke()
        }
    }
}

private fun ContentPermission.toTrackingProtectionException(): GeckoTrackingProtectionException {
    return GeckoTrackingProtectionException(uri, privateMode, this)
}
