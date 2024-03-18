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
 * @property url The loading or loaded URL.
 * @property private Whether or not the session is private.
 * @property title The title of the current page.
 * @property progress The loading progress of the current page denoted as 0-100.
 * @property loading True if state is loading.
 * @property searchTerms The last used search terms, or an empty string if no
 * search was executed for this session.
 * @property securityInfo The security information as [SecurityInfoState],
 * describing whether or not the this session is for a secure URL, as well
 * as the host and SSL certificate authority.
 * @property icon The icon of the page currently loaded by this session.
 * @property download Last unhandled download request.
 * @property share Last unhandled request to share an internet resource that first needs to be downloaded.
 * @property copy Last unhandled request to copy an internet resource that first needs to be downloaded.
 * @property hitResult The target of the latest long click operation.
 * @property promptRequests Current[PromptRequest]s.
 * @property findResults The list of results of the latest "find in page" operation.
 * @property windowRequest The last received [WindowRequest].
 * @property searchRequest The last received [SearchRequest].
 * @property fullScreen True if the page is full screen, false if not.
 * @property layoutInDisplayCutoutMode The display layout cutout mode state.
 * @property canGoBack Whether or not there's a history item to navigate back to.
 * @property canGoForward Whether or not there's a history item to navigate forward to.
 * @property webAppManifest The Web App Manifest for the currently visited page (or null).
 * @property firstContentfulPaint Whether or not the first contentful paint has happened.
 * @property history The [HistoryState] of this state.
 * @property permissionHighlights Holds the state of any site permission that was granted/denied
 * that should be brought to the user's attention, for example when media content is not able to
 * play because the autoplay settings.
 * @property permissionRequestsList Holds unprocessed content requests.
 * @property appPermissionRequestsList Holds unprocessed app requests.
 * @property pictureInPictureEnabled True if the session is being displayed in PIP mode.
 * @property loadRequest The last [LoadRequestState] of this session.
 * @property refreshCanceled Indicates if an intent of refreshing was canceled.
 * True if a page refresh was cancelled by the user, defaults to false. Note that this is not about
 * stopping an ongoing page load but useful in cases like swipe-to-refresh which allow users to
 * cancel or abort before a page is refreshed.
 * @property recordingDevices List of recording devices (e.g. camera or microphone) currently in use
 * by web content.
 * @property desktopMode True if desktop mode is enabled, otherwise false.
 * @property appIntent The last received [AppIntentState].
 * @property showToolbarAsExpanded Whether the dynamic toolbar should be forced as expanded.
 * @property previewImageUrl The preview image of the page (e.g. the hero image), if available.
 * @property isSearch Whether or not the last url load request is the result of a search.
 * @property hasFormData Whether or not the content has filled out form data.
 * @property isProductUrl Indicates if the [url] is a supported product page.
 */
data class ContentState(
    val url: String,
    val private: Boolean = false,
    val title: String = "",
    val progress: Int = 0,
    val loading: Boolean = false,
    val searchTerms: String = "",
    val securityInfo: SecurityInfoState = SecurityInfoState(),
    val icon: Bitmap? = null,
    val download: DownloadState? = null,
    val share: ShareInternetResourceState? = null,
    val copy: ShareInternetResourceState? = null,
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
    val showToolbarAsExpanded: Boolean = false,
    val previewImageUrl: String? = null,
    val isSearch: Boolean = false,
    val hasFormData: Boolean = false,
    val isProductUrl: Boolean = false,
)
