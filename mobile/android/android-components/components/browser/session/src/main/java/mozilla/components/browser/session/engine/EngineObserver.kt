/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine

import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Environment
import androidx.core.net.toUri
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LoadRequestState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.state.Store
import mozilla.components.support.ktx.android.net.isInScope
import mozilla.components.support.ktx.kotlin.isSameOriginAs

/**
 * [EngineSession.Observer] implementation responsible to update the state of a [Session] from the events coming out of
 * an [EngineSession].
 */
@Suppress("TooManyFunctions", "LargeClass")
internal class EngineObserver(
    private val session: Session,
    private val store: Store<BrowserState, BrowserAction>?
) : EngineSession.Observer {
    override fun onNavigateBack() {
        store?.dispatch(ContentAction.UpdateSearchTermsAction(session.id, ""))
    }

    override fun onFirstContentfulPaint() {
        store?.dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(session.id, true))
    }

    override fun onPaintStatusReset() {
        store?.dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(session.id, false))
    }

    override fun onLocationChange(url: String) {
        if (!isUrlSame(session.url, url)) {
            session.title = ""
        }

        if (!isInScope(session.webAppManifest, url)) {
            session.webAppManifest = null
        }

        if (!session.url.isSameOriginAs(url)) {
            store?.dispatch(ContentAction.ClearPermissionRequests(session.id))
        }

        session.url = url
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
        val scopeUri = scope.toUri()
        val newUri = newUrl.toUri()

        return newUri.isInScope(listOf(scopeUri))
    }

    @Suppress("DEPRECATION") // Session observable is deprecated
    override fun onLoadRequest(
        url: String,
        triggeredByRedirect: Boolean,
        triggeredByWebContent: Boolean
    ) {
        if (triggeredByRedirect || triggeredByWebContent) {
            store?.dispatch(ContentAction.UpdateSearchTermsAction(session.id, ""))
        }

        session.notifyObservers {
            onLoadRequest(session, url, triggeredByRedirect, triggeredByWebContent)
        }

        val loadRequest = LoadRequestState(url, triggeredByRedirect, triggeredByWebContent)
        store?.dispatch(ContentAction.UpdateLoadRequestAction(session.id, loadRequest))
    }

    override fun onLaunchIntentRequest(url: String, appIntent: Intent?) {
        store?.dispatch(ContentAction.UpdateAppIntentAction(session.id, AppIntentState(url, appIntent)))
    }

    override fun onTitleChange(title: String) {
        session.title = title
    }

    override fun onProgress(progress: Int) {
        session.progress = progress
    }

    override fun onLoadingStateChange(loading: Boolean) {
        session.loading = loading
        if (loading) {
            store?.dispatch(ContentAction.ClearFindResultsAction(session.id))
            store?.dispatch(ContentAction.UpdateRefreshCanceledStateAction(session.id, false))

            session.trackersBlocked = emptyList()
            session.trackersLoaded = emptyList()
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let {
            store?.syncDispatch(ContentAction.UpdateBackNavigationStateAction(session.id, canGoBack))
        }
        canGoForward?.let {
            store?.syncDispatch(ContentAction.UpdateForwardNavigationStateAction(session.id, canGoForward))
        }
    }

    override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
        session.securityInfo = Session.SecurityInfo(secure, host
                ?: "", issuer ?: "")
    }

    override fun onTrackerBlocked(tracker: Tracker) {
        session.trackersBlocked += tracker
    }

    override fun onTrackerLoaded(tracker: Tracker) {
        session.trackersLoaded += tracker
    }

    override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
        store?.dispatch(TrackingProtectionAction.ToggleExclusionListAction(session.id, excluded))
    }

    override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
        session.trackerBlockingEnabled = enabled
    }

    override fun onLongPress(hitResult: HitResult) {
        store?.dispatch(
            ContentAction.UpdateHitResultAction(session.id, hitResult)
        )
    }

    override fun onFind(text: String) {
        store?.dispatch(ContentAction.ClearFindResultsAction(session.id))
    }

    override fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
        store?.dispatch(ContentAction.AddFindResultAction(
            session.id,
            FindResultState(
                activeMatchOrdinal,
                numberOfMatches,
                isDoneCounting
            )
        ))
    }

    override fun onExternalResource(
        url: String,
        fileName: String?,
        contentLength: Long?,
        contentType: String?,
        cookie: String?,
        userAgent: String?,
        isPrivate: Boolean,
        response: Response?
    ) {
        // We want to avoid negative contentLength values
        // For more info see https://bugzilla.mozilla.org/show_bug.cgi?id=1632594
        val fileSize = if (contentLength != null && contentLength < 0) null else contentLength
        val download = DownloadState(
            url,
            fileName,
            contentType,
            fileSize,
            0,
            INITIATED,
            userAgent,
            Environment.DIRECTORY_DOWNLOADS,
            private = isPrivate,
            response = response
        )

        store?.dispatch(ContentAction.UpdateDownloadAction(
            session.id,
            download
        ))
    }

    override fun onDesktopModeChange(enabled: Boolean) {
        store?.dispatch(ContentAction.UpdateDesktopModeAction(
            session.id, enabled
        ))
    }

    override fun onFullScreenChange(enabled: Boolean) {
        store?.dispatch(ContentAction.FullScreenChangedAction(
            session.id, enabled
        ))
    }

    override fun onMetaViewportFitChanged(layoutInDisplayCutoutMode: Int) {
        store?.dispatch(ContentAction.ViewportFitChangedAction(
            session.id, layoutInDisplayCutoutMode
        ))
    }

    override fun onThumbnailChange(bitmap: Bitmap?) {
        store?.dispatch(if (bitmap == null) {
                ContentAction.RemoveThumbnailAction(session.id)
            } else {
                ContentAction.UpdateThumbnailAction(session.id, bitmap)
            }
        )
    }

    override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
        store?.dispatch(
            ContentAction.UpdatePermissionsRequest(
                session.id,
                permissionRequest
            )
        )
    }

    override fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) {
        store?.dispatch(
            ContentAction.ConsumePermissionsRequest(
                session.id,
                permissionRequest
            )
        )
    }

    override fun onAppPermissionRequest(permissionRequest: PermissionRequest) {
        store?.dispatch(
            ContentAction.UpdateAppPermissionsRequest(
                session.id,
                permissionRequest
            )
        )
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        store?.dispatch(ContentAction.UpdatePromptRequestAction(
            session.id,
            promptRequest
        ))
    }

    override fun onRepostPromptCancelled() {
        store?.dispatch(ContentAction.UpdateRefreshCanceledStateAction(session.id, true))
    }

    override fun onBeforeUnloadPromptDenied() {
        store?.dispatch(ContentAction.UpdateRefreshCanceledStateAction(session.id, true))
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        store?.dispatch(
            ContentAction.UpdateWindowRequestAction(
                session.id,
                windowRequest
            )
        )
    }

    override fun onMediaActivated(mediaSessionController: MediaSession.Controller) {
        store?.dispatch(MediaSessionAction.ActivatedMediaSessionAction(
            session.id,
            mediaSessionController
        ))
    }

    override fun onMediaDeactivated() {
        store?.dispatch(MediaSessionAction.DeactivatedMediaSessionAction(session.id))
    }

    override fun onMediaMetadataChanged(metadata: MediaSession.Metadata) {
        store?.dispatch(MediaSessionAction.UpdateMediaMetadataAction(session.id, metadata))
    }

    override fun onMediaPlaybackStateChanged(playbackState: MediaSession.PlaybackState) {
        store?.dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(
            session.id,
            playbackState
        ))
    }

    override fun onMediaFeatureChanged(features: MediaSession.Feature) {
        store?.dispatch(MediaSessionAction.UpdateMediaFeatureAction(
            session.id,
            features
        ))
    }

    override fun onMediaPositionStateChanged(positionState: MediaSession.PositionState) {
        store?.dispatch(MediaSessionAction.UpdateMediaPositionStateAction(
            session.id,
            positionState
        ))
    }

    override fun onMediaMuteChanged(muted: Boolean) {
        store?.dispatch(MediaSessionAction.UpdateMediaMutedAction(
            session.id,
            muted
        ))
    }

    override fun onMediaFullscreenChanged(
        fullscreen: Boolean,
        elementMetadata: MediaSession.ElementMetadata?
    ) {
        store?.dispatch(MediaSessionAction.UpdateMediaFullscreenAction(
            session.id,
            fullscreen,
            elementMetadata
        ))
    }

    override fun onWebAppManifestLoaded(manifest: WebAppManifest) {
        session.webAppManifest = manifest
    }

    override fun onCrash() {
        store?.dispatch(CrashAction.SessionCrashedAction(
            session.id
        ))
    }

    override fun onProcessKilled() {
        store?.dispatch(EngineAction.KillEngineSessionAction(
            session.id
        ))
    }

    override fun onStateUpdated(state: EngineSessionState) {
        store?.dispatch(EngineAction.UpdateEngineSessionStateAction(
            session.id, state
        ))
    }

    override fun onRecordingStateChanged(devices: List<RecordingDevice>) {
        store?.dispatch(ContentAction.SetRecordingDevices(
            session.id, devices
        ))
    }

    override fun onHistoryStateChanged(historyList: List<HistoryItem>, currentIndex: Int) {
        store?.dispatch(ContentAction.UpdateHistoryStateAction(
            session.id,
            historyList,
            currentIndex
        ))
    }
}
