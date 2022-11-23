/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.cookiebanners

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.engine.gecko.await
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.DISABLED
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.StorageController

/**
 * A storage to store [CookieBannerHandlingMode] using GeckoView APIs.
 */
class GeckoCookieBannersStorage(
    runtime: GeckoRuntime,
) : CookieBannersStorage {

    private val geckoStorage: StorageController = runtime.storageController
    private val mainScope = CoroutineScope(Dispatchers.Main)

    override suspend fun addException(
        uri: String,
        privateBrowsing: Boolean,
    ) {
        setGeckoException(uri, DISABLED, privateBrowsing)
    }

    override suspend fun findExceptionFor(
        uri: String,
        privateBrowsing: Boolean,
    ): CookieBannerHandlingMode {
        return queryExceptionInGecko(uri, privateBrowsing)
    }

    override suspend fun hasException(uri: String, privateBrowsing: Boolean): Boolean {
        return findExceptionFor(uri, privateBrowsing) == DISABLED
    }

    override suspend fun removeException(uri: String, privateBrowsing: Boolean) {
        removeGeckoException(uri, privateBrowsing)
    }

    @VisibleForTesting
    internal fun removeGeckoException(uri: String, privateBrowsing: Boolean) {
        geckoStorage.removeCookieBannerModeForDomain(uri, privateBrowsing)
    }

    @VisibleForTesting
    internal fun setGeckoException(
        uri: String,
        mode: CookieBannerHandlingMode,
        privateBrowsing: Boolean,
    ) {
        geckoStorage.setCookieBannerModeForDomain(
            uri,
            mode.mode,
            privateBrowsing,
        )
    }

    @VisibleForTesting
    internal suspend fun queryExceptionInGecko(
        uri: String,
        privateBrowsing: Boolean,
    ): CookieBannerHandlingMode {
        return withContext(mainScope.coroutineContext) {
            geckoStorage.getCookieBannerModeForDomain(uri, privateBrowsing).await()
                ?.toCookieBannerHandlingMode() ?: throw IllegalArgumentException(
                "An error happened trying to find cookie banners mode for the " +
                    "uri $uri and private browsing mode $privateBrowsing",
            )
        }
    }
}

@VisibleForTesting
internal fun Int.toCookieBannerHandlingMode(): CookieBannerHandlingMode {
    return CookieBannerHandlingMode.values().first { it.mode == this }
}
