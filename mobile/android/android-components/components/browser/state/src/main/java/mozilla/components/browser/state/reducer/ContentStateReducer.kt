/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import android.net.Uri
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.ContentAction.UpdatePermissionHighlightsStateAction
import mozilla.components.browser.state.ext.containsPermission
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.HistoryState
import mozilla.components.browser.state.state.content.PermissionHighlightsState
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.ktx.android.net.isInScope
import mozilla.components.support.ktx.android.net.sameSchemeAndHostAs
import mozilla.components.support.ktx.kotlin.isSameOriginAs

@Suppress("LargeClass")
internal object ContentStateReducer {
    /**
     * [ContentAction] Reducer function for modifying a specific [ContentState] of a [SessionState].
     */
    @Suppress("LongMethod")
    fun reduce(state: BrowserState, action: ContentAction): BrowserState {
        return when (action) {
            is ContentAction.RemoveIconAction -> updateContentState(state, action.sessionId) {
                it.copy(icon = null)
            }
            is ContentAction.RemoveThumbnailAction -> updateContentState(state, action.sessionId) {
                it.copy(thumbnail = null)
            }
            is ContentAction.UpdateUrlAction -> updateContentState(state, action.sessionId) {
                it.copy(
                    url = action.url,
                    icon = if (!isHostEquals(it.url, action.url)) {
                        null
                    } else {
                        it.icon
                    },
                    title = if (!isUrlSame(it.url, action.url)) {
                        ""
                    } else {
                        it.title
                    },
                    webAppManifest = if (!isInScope(it.webAppManifest, action.url)) {
                        null
                    } else {
                        it.webAppManifest
                    },
                    permissionRequestsList = if (!it.url.isSameOriginAs(action.url)) {
                        emptyList()
                    } else {
                        it.permissionRequestsList
                    }
                )
            }
            is ContentAction.UpdateProgressAction -> updateContentState(state, action.sessionId) {
                it.copy(progress = action.progress)
            }
            is ContentAction.UpdateTitleAction -> updateContentState(state, action.sessionId) {
                it.copy(title = action.title)
            }
            is ContentAction.UpdateLoadingStateAction -> updateContentState(state, action.sessionId) {
                it.copy(loading = action.loading)
            }
            is ContentAction.UpdateRefreshCanceledStateAction -> updateContentState(state, action.sessionId) {
                it.copy(refreshCanceled = action.refreshCanceled)
            }
            is ContentAction.UpdateSearchTermsAction -> updateContentState(state, action.sessionId) {
                it.copy(searchTerms = action.searchTerms)
            }
            is ContentAction.UpdateSecurityInfoAction -> updateContentState(state, action.sessionId) {
                it.copy(securityInfo = action.securityInfo)
            }
            is ContentAction.UpdateIconAction -> updateContentState(state, action.sessionId) {
                if (action.pageUrl == it.url) {
                    // Only update the icon of the state if we are still on this page. The user may
                    // have navigated away by the time the icon is loaded.
                    it.copy(icon = action.icon)
                } else {
                    it
                }
            }
            is ContentAction.UpdateThumbnailAction -> updateContentState(state, action.sessionId) {
                it.copy(thumbnail = action.thumbnail)
            }
            is ContentAction.UpdateDownloadAction -> updateContentState(state, action.sessionId) {
                it.copy(download = action.download.copy(sessionId = action.sessionId))
            }
            is ContentAction.ConsumeDownloadAction -> consumeDownload(state, action.sessionId, action.downloadId)
            is ContentAction.CancelDownloadAction -> consumeDownload(state, action.sessionId, action.downloadId)

            is ContentAction.UpdateHitResultAction -> updateContentState(state, action.sessionId) {
                it.copy(hitResult = action.hitResult)
            }
            is ContentAction.ConsumeHitResultAction -> updateContentState(state, action.sessionId) {
                it.copy(hitResult = null)
            }
            is ContentAction.UpdatePromptRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(promptRequests = it.promptRequests + action.promptRequest)
            }
            is ContentAction.ConsumePromptRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(promptRequests = it.promptRequests - action.promptRequest)
            }
            is ContentAction.AddFindResultAction -> updateContentState(state, action.sessionId) {
                it.copy(findResults = it.findResults + action.findResult)
            }
            is ContentAction.ClearFindResultsAction -> updateContentState(state, action.sessionId) {
                it.copy(findResults = emptyList())
            }
            is ContentAction.UpdateWindowRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(windowRequest = action.windowRequest)
            }
            is ContentAction.ConsumeWindowRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(windowRequest = null)
            }
            is ContentAction.UpdateSearchRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(searchRequest = action.searchRequest)
            }
            is ContentAction.ConsumeSearchRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(searchRequest = null)
            }
            is ContentAction.FullScreenChangedAction -> updateContentState(state, action.sessionId) {
                it.copy(fullScreen = action.fullScreenEnabled)
            }
            is ContentAction.PictureInPictureChangedAction -> updateContentState(state, action.sessionId) {
                it.copy(pictureInPictureEnabled = action.pipEnabled)
            }
            is ContentAction.ViewportFitChangedAction -> updateContentState(state, action.sessionId) {
                it.copy(layoutInDisplayCutoutMode = action.layoutInDisplayCutoutMode)
            }
            is ContentAction.UpdateBackNavigationStateAction -> updateContentState(state, action.sessionId) {
                it.copy(canGoBack = action.canGoBack)
            }
            is ContentAction.UpdateForwardNavigationStateAction -> updateContentState(state, action.sessionId) {
                it.copy(canGoForward = action.canGoForward)
            }
            is ContentAction.UpdateWebAppManifestAction -> updateContentState(state, action.sessionId) {
                it.copy(webAppManifest = action.webAppManifest)
            }
            is ContentAction.RemoveWebAppManifestAction -> updateContentState(state, action.sessionId) {
                it.copy(webAppManifest = null)
            }
            is ContentAction.UpdateFirstContentfulPaintStateAction -> updateContentState(state, action.sessionId) {
                it.copy(firstContentfulPaint = action.firstContentfulPaint)
            }
            is ContentAction.UpdateHistoryStateAction -> updateContentState(state, action.sessionId) {
                it.copy(history = HistoryState(action.historyList, action.currentIndex))
            }
            is ContentAction.UpdatePermissionsRequest -> updateContentState(
                state,
                action.sessionId
            ) {
                if (!it.permissionRequestsList.containsPermission(action.permissionRequest)) {
                    it.copy(
                        permissionRequestsList = it.permissionRequestsList + action.permissionRequest
                    )
                } else {
                    it
                }
            }
            is ContentAction.ConsumePermissionsRequest -> updateContentState(
                state,
                action.sessionId
            ) {
                if (it.permissionRequestsList.containsPermission(action.permissionRequest)) {
                    it.copy(
                        permissionRequestsList = it.permissionRequestsList - action.permissionRequest
                    )
                } else {
                    it
                }
            }
            is ContentAction.UpdateAppPermissionsRequest -> updateContentState(
                state,
                action.sessionId
            ) {
                if (!it.appPermissionRequestsList.containsPermission(action.appPermissionRequest)) {
                    it.copy(
                        appPermissionRequestsList = it.appPermissionRequestsList + action.appPermissionRequest
                    )
                } else {
                    it
                }
            }
            is ContentAction.ConsumeAppPermissionsRequest -> updateContentState(
                state,
                action.sessionId
            ) {
                if (it.appPermissionRequestsList.containsPermission(action.appPermissionRequest)) {
                    it.copy(
                        appPermissionRequestsList = it.appPermissionRequestsList - action.appPermissionRequest
                    )
                } else {
                    it
                }
            }
            is ContentAction.ClearPermissionRequests -> updateContentState(
                state,
                action.sessionId
            ) {
                it.copy(permissionRequestsList = emptyList())
            }
            is ContentAction.ClearAppPermissionRequests -> updateContentState(
                state,
                action.sessionId
            ) {
                it.copy(appPermissionRequestsList = emptyList())
            }
            is ContentAction.UpdateLoadRequestAction -> updateContentState(state, action.sessionId) {
                it.copy(loadRequest = action.loadRequest)
            }
            is ContentAction.SetRecordingDevices -> updateContentState(state, action.sessionId) {
                it.copy(recordingDevices = action.devices)
            }
            is ContentAction.UpdateDesktopModeAction -> updateContentState(state, action.sessionId) {
                it.copy(desktopMode = action.enabled)
            }
            is UpdatePermissionHighlightsStateAction.NotificationChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(notificationChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.CameraChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(cameraChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.LocationChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(locationChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.MediaKeySystemAccesChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(mediaKeySystemAccessChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.MicrophoneChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(microphoneChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.PersistentStorageChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(persistentStorageChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.AutoPlayAudibleBlockingAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(autoPlayAudibleBlocking = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.AutoPlayInAudibleChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(autoPlayInaudibleChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.AutoPlayInAudibleBlockingAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(autoPlayInaudibleBlocking = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.AutoPlayAudibleChangedAction -> {
                updatePermissionHighlightsState(state, action.tabId) {
                    it.copy(autoPlayAudibleChanged = action.value)
                }
            }
            is UpdatePermissionHighlightsStateAction.Reset -> {
                updatePermissionHighlightsState(state, action.tabId) { PermissionHighlightsState() }
            }

            is ContentAction.UpdateAppIntentAction -> updateContentState(state, action.sessionId) {
                it.copy(appIntent = action.appIntent)
            }
            is ContentAction.ConsumeAppIntentAction -> updateContentState(state, action.sessionId) {
                it.copy(appIntent = null)
            }
            is ContentAction.UpdateExpandedToolbarStateAction -> updateContentState(state, action.sessionId) {
                it.copy(showToolbarAsExpanded = action.expanded)
            }
        }
    }
    private fun consumeDownload(state: BrowserState, sessionId: String, downloadId: String): BrowserState {
        return updateContentState(state, sessionId) {
            if (it.download != null && it.download.id == downloadId) {
                it.copy(download = null)
            } else {
                it
            }
        }
    }
}

private inline fun updatePermissionHighlightsState(
    state: BrowserState,
    tabId: String,
    crossinline update: (PermissionHighlightsState) -> PermissionHighlightsState
): BrowserState {
    return updateContentState(state, tabId) {
        it.copy(permissionHighlights = update(it.permissionHighlights))
    }
}

private inline fun updateContentState(
    state: BrowserState,
    tabId: String,
    crossinline update: (ContentState) -> ContentState
): BrowserState {
    return state.updateTabOrCustomTabState(tabId) { current ->
        current.createCopy(content = update(current.content))
    }
}

private fun isHostEquals(sessionUrl: String, newUrl: String): Boolean {
    val sessionUri = Uri.parse(sessionUrl)
    val newUri = Uri.parse(newUrl)

    return sessionUri.sameSchemeAndHostAs(newUri)
}

private fun isUrlSame(originalUrl: String, newUrl: String): Boolean {
    val originalUri = Uri.parse(originalUrl)
    val uri = Uri.parse(newUrl)

    return uri.port == originalUri.port &&
        uri.host == originalUri.host &&
        uri.path?.trimStart('/') == originalUri.path?.trimStart('/') &&
        uri.query == originalUri.query
}

/**
 * Checks that the [newUrl] is in scope of the web app manifest.
 *
 * https://www.w3.org/TR/appmanifest/#dfn-within-scope
 */
private fun isInScope(manifest: WebAppManifest?, newUrl: String): Boolean {
    val scope = manifest?.scope ?: manifest?.startUrl ?: return false
    val scopeUri = Uri.parse(scope)
    val newUri = Uri.parse(newUrl)

    return newUri.isInScope(listOf(scopeUri))
}
