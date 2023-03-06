/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.temporary

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.shareMedia
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * At most time to allow for the file to be downloaded and action to be performed.
 */
private const val OPERATION_TIMEOUT_MS: Long = 1000L

/**
 * [LifecycleAwareFeature] implementation for sharing online resources.
 *
 * This will intercept only [ShareInternetResourceAction] [BrowserAction]s.
 *
 * Following which it will transparently
 *  - download internet resources while respecting the private mode related to cookies handling
 *  - temporarily cache the downloaded resources
 *  - automatically open the platform app chooser to share the cached files with other installed Android apps.
 *
 * with a 1 second timeout to ensure a smooth UX.
 *
 * To finish the process in this small timeframe the feature is recommended to be used only for images.
 *
 *  @property context Android context used for various platform interactions
 *  @property store a reference to the application's [BrowserStore]
 *  @property tabId ID of the tab session, or null if the selected session should be used.
 *  @param httpClient Client used for downloading internet resources
 *  @param cleanupCacheCoroutineDispatcher Coroutine dispatcher used for the cleanup of old
 *  cached files. Defaults to IO.
 */
class ShareDownloadFeature(
    private val context: Context,
    private val store: BrowserStore,
    private val tabId: String?,
    httpClient: Client,
    cleanupCacheCoroutineDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : TemporaryDownloadFeature(context, httpClient, cleanupCacheCoroutineDispatcher) {

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifChanged { it.content.share }
                .collect { state ->
                    state.content.share?.let { shareState ->
                        logger.debug("Starting the sharing process")
                        startSharing(shareState)

                        // This is a fire and forget action, not something that we want lingering the tab state.
                        store.dispatch(ShareInternetResourceAction.ConsumeShareAction(state.id))
                    }
                }
        }
    }

    @VisibleForTesting
    internal fun startSharing(internetResource: ShareInternetResourceState) {
        val coroutineExceptionHandler = coroutineExceptionHandler("Share")

        scope?.launch(coroutineExceptionHandler) {
            withTimeout(OPERATION_TIMEOUT_MS) {
                val download = download(internetResource)
                share(
                    contentType = internetResource.contentType,
                    filePath = download.canonicalPath,
                )
            }
        }
    }

    @VisibleForTesting
    internal fun share(
        filePath: String,
        contentType: String?,
        subject: String? = null,
        message: String? = null,
    ) = context.shareMedia(filePath, contentType, subject, message)
}
