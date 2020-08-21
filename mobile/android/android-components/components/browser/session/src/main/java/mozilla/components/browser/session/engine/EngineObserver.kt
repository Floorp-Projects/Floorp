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
import mozilla.components.browser.session.engine.request.LaunchIntentMetadata
import mozilla.components.browser.session.engine.request.LoadRequestMetadata
import mozilla.components.browser.session.engine.request.LoadRequestOption
import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.session.ext.toElement
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.ktx.android.net.isInScope
import mozilla.components.support.ktx.kotlin.isSameOriginAs

/**
 * [EngineSession.Observer] implementation responsible to update the state of a [Session] from the events coming out of
 * an [EngineSession].
 */
@Suppress("TooManyFunctions", "LargeClass")
internal class EngineObserver(
    private val session: Session,
    private val store: BrowserStore? = null
) : EngineSession.Observer {
    private val mediaMap: MutableMap<Media, MediaObserver> = mutableMapOf()

    override fun onNavigateBack() {
        session.searchTerms = ""
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
            session.contentPermissionRequest.consume {
                it.reject()
                true
            }
        }
        session.url = url

        // Meh, GeckoView doesn't notify us about recording devices no longer used when navigating away. As a
        // workaround we clear them here. But that's not perfect...
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1554778
        session.recordingDevices = listOf()
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

    override fun onLoadRequest(
        url: String,
        triggeredByRedirect: Boolean,
        triggeredByWebContent: Boolean
    ) {
        if (triggeredByRedirect || triggeredByWebContent) {
            session.searchTerms = ""
        }

        session.loadRequestMetadata = LoadRequestMetadata(
            url,
            arrayOf(
                if (triggeredByRedirect) LoadRequestOption.REDIRECT else LoadRequestOption.NONE,
                if (triggeredByWebContent) LoadRequestOption.WEB_CONTENT else LoadRequestOption.NONE
            )
        )
    }

    override fun onLaunchIntentRequest(url: String, appIntent: Intent?) {
        session.launchIntentMetadata = LaunchIntentMetadata(url, appIntent)
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

            session.trackersBlocked = emptyList()
            session.trackersLoaded = emptyList()
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { session.canGoBack = canGoBack }
        canGoForward?.let { session.canGoForward = canGoForward }
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
        store?.syncDispatch(TrackingProtectionAction.ToggleExclusionListAction(session.id, excluded))
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
        userAgent: String?
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
            Environment.DIRECTORY_DOWNLOADS
        )

        store?.dispatch(ContentAction.UpdateDownloadAction(
            session.id,
            download
        ))
    }

    override fun onDesktopModeChange(enabled: Boolean) {
        session.desktopMode = enabled
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
        session.thumbnail = bitmap
    }

    override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
        session.contentPermissionRequest = Consumable.from(permissionRequest)
    }

    override fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) {
        session.contentPermissionRequest = Consumable.empty()
    }

    override fun onAppPermissionRequest(permissionRequest: PermissionRequest) {
        session.appPermissionRequest = Consumable.from(permissionRequest)
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        store?.dispatch(ContentAction.UpdatePromptRequestAction(
            session.id,
            promptRequest
        ))
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        store?.dispatch(
            ContentAction.UpdateWindowRequestAction(
                session.id,
                windowRequest
            )
        )
    }

    override fun onMediaAdded(media: Media) {
        val store = store ?: return

        val mediaElement = media.toElement()

        store.dispatch(MediaAction.AddMediaAction(session.id, mediaElement))

        val observer = MediaObserver(media, mediaElement, store, session.id)
        media.register(observer)

        mediaMap[media] = observer
    }

    override fun onMediaRemoved(media: Media) {
        val store = store ?: return

        val observer = mediaMap[media]
        if (observer != null) {
            media.unregister(observer)

            store.dispatch(MediaAction.RemoveMediaAction(session.id, observer.element))
        }

        mediaMap.remove(media)
    }

    override fun onWebAppManifestLoaded(manifest: WebAppManifest) {
        session.webAppManifest = manifest
    }

    override fun onCrash() {
        session.crashed = true
    }

    override fun onRecordingStateChanged(devices: List<RecordingDevice>) {
        session.recordingDevices = devices
    }

    override fun onHistoryStateChanged(historyList: List<HistoryItem>, currentIndex: Int) {
        store?.dispatch(ContentAction.UpdateHistoryStateAction(
            session.id,
            historyList,
            currentIndex
        ))
    }
}
