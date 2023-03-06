/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.temporary

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CopyInternetResourceAction
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.copyImage
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.support.utils.SnackbarDelegate
import java.util.concurrent.TimeUnit

/**
 * [LifecycleAwareFeature] implementation for copying online resources.
 *
 * This will intercept only [ShareInternetResourceAction] [BrowserAction]s.
 *
 * Following which it will transparently
 *  - download internet resources while respecting the private mode related to cookies handling
 *  - temporarily cache the downloaded resources
 *  - copy the resource to the device clipboard.
 *
 * with a 1 second timeout to ensure a smooth UX.
 *
 * To finish the process in this small timeframe the feature is recommended to be used only for images.
 *
 *  @property context Android context used for various platform interactions.
 *  @property store a reference to the application's [BrowserStore].
 *  @property tabId ID of the tab session, or null if the selected session should be used.
 *  @property snackbarParent Parent [View] of the [SnackbarDelegate].
 *  @property snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
 *  @param httpClient Client used for downloading internet resources.
 *  @param cleanupCacheCoroutineDispatcher Coroutine dispatcher used for the cleanup of old
 *  cached files. Defaults to IO.
 */
class CopyDownloadFeature(
    private val context: Context,
    private val store: BrowserStore,
    private val tabId: String?,
    private val snackbarParent: View,
    private val snackbarDelegate: SnackbarDelegate,
    httpClient: Client,
    cleanupCacheCoroutineDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : TemporaryDownloadFeature(
    context,
    httpClient,
    cleanupCacheCoroutineDispatcher,
) {

    /**
     * At most time to allow for the file to be downloaded.
     */
    private val operationTimeoutMs by lazy { TimeUnit.MINUTES.toMinutes(1) }

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifChanged { it.content.copy }
                .collect { state ->
                    state.content.copy?.let { copyState ->
                        logger.debug("Starting the copying process")
                        startCopy(copyState)

                        // This is a fire and forget action, not something that we want lingering the tab state.
                        store.dispatch(CopyInternetResourceAction.ConsumeCopyAction(state.id))
                    }
                }
        }
    }

    @VisibleForTesting
    internal fun startCopy(internetResource: ShareInternetResourceState) {
        val coroutineExceptionHandler = coroutineExceptionHandler("Copy")

        scope?.launch(coroutineExceptionHandler) {
            withTimeout(operationTimeoutMs) {
                val download = download(internetResource)
                copy(download.canonicalPath, snackbarParent, snackbarDelegate)
            }
        }
    }

    @VisibleForTesting
    internal fun copy(filePath: String, snackbarParent: View, snackbarDelegate: SnackbarDelegate) =
        context.copyImage(filePath, snackbarParent, snackbarDelegate)
}
