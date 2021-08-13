/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.graphics.Bitmap
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.state.content.HistoryState
import mozilla.components.browser.state.state.content.PermissionHighlightsState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.concept.engine.window.WindowRequest

/**
 * Value type that represents the state of the content within a [SessionState].
 *
 * @property url the loading or loaded URL.
 * @property private whether or not the session is private.
 * @property title the title of the current page.
 * @property progress the loading progress of the current page.
 * @property searchTerms the last used search terms, or an empty string if no
 * search was executed for this session.
 * @property securityInfo the security information as [SecurityInfoState],
 * describing whether or not the this session is for a secure URL, as well
 * as the host and SSL certificate authority.
 * @property thumbnail the last generated [Bitmap] of this session's content, to
 * be used as a preview in e.g. a tab switcher.
 * @property icon the icon of the page currently loaded by this session.
 * @property download Last unhandled download request.
 * @property share Last unhandled request to share an internet resource that first needs to be downloaded.
 * @property hitResult the target of the latest long click operation.
 * @property promptRequests current[PromptRequest]s.
 * @property findResults the list of results of the latest "find in page" operation.
 * @property windowRequest the last received [WindowRequest].
 * @property searchRequest the last received [SearchRequest]
 * @property fullScreen true if the page is full screen, false if not.
 * @property layoutInDisplayCutoutMode the display layout cutout mode state.
 * @property canGoBack whether or not there's an history item to navigate back to.
 * @property canGoForward whether or not there's an history item to navigate forward to.
 * @property webAppManifest the Web App Manifest for the currently visited page (or null).
 * @property firstContentfulPaint whether or not the first contentful paint has happened.
 * @property pictureInPictureEnabled True if the session is being displayed in PIP mode.
 * @property loadRequest last [LoadRequestState] if this session.
 * @property permissionIndicator Holds the state of any site permission that was granted/denied
 * that should be brought to the user's attention, for example when media content is not able to
 * play because the autoplay settings.
 * @property appPermissionRequestsList Holds unprocessed app requests.
 * @property refreshCanceled Indicates if an intent of refreshing was canceled.
 * True if a page refresh was cancelled by the user, defaults to false. Note that this is not about
 * stopping an ongoing page load but useful in cases like swipe-to-refresh which allow users to
 * cancel or abort before a page is refreshed.
 * @property recordingDevices List of recording devices (e.g. camera or microphone) currently in use
 * by web content.
 * @property desktopMode true if desktop mode is enabled, otherwise false.
 * @property appIntent the last received [AppIntentState].
 * @property showToolbarAsExpanded whether the dynamic toolbar should be forced as expanded.
 */
data class ContentState(
    val url: String,
    val private: Boolean = false,
    val title: String = "",
    val progress: Int = 0,
    val loading: Boolean = false,
    val searchTerms: String = "",
    val securityInfo: SecurityInfoState = SecurityInfoState(),
    val thumbnail: Bitmap? = null,
    val icon: Bitmap? = null,
    val download: DownloadState? = null,
    val share: ShareInternetResourceState? = null,
    val hitResult: HitResult? = null,
    val promptRequests: List<PromptRequest> = emptyList(),
    val findResults: List<FindResultState> = emptyList(),
    val windowRequest: WindowRequest? = null,
    val searchRequest: SearchRequest? = null,
    val fullScreen: Boolean = false,
    val layoutInDisplayCutoutMode: Int = 0,
    val canGoBack: Boolean = false,
    val canGoForward: Boolean = false,
    val webAppManifest: WebAppManifest? = null,
    val firstContentfulPaint: Boolean = false,
    val history: HistoryState = HistoryState(),
    val permissionHighlights: PermissionHighlightsState = PermissionHighlightsState(),
    val permissionRequestsList: List<PermissionRequest> = emptyList(),
    val appPermissionRequestsList: List<PermissionRequest> = emptyList(),
    val pictureInPictureEnabled: Boolean = false,
    val loadRequest: LoadRequestState? = null,
    val refreshCanceled: Boolean = false,
    val recordingDevices: List<RecordingDevice> = emptyList(),
    val desktopMode: Boolean = false,
    val appIntent: AppIntentState? = null,
    val showToolbarAsExpanded: Boolean = false
)
