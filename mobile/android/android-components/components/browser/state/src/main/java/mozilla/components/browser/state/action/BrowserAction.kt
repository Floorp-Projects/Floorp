/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import android.graphics.Bitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.Media
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.lib.state.Action

/**
 * [Action] implementation related to [BrowserState].
 */
sealed class BrowserAction : Action

/**
 * [BrowserAction] implementations to react to system events.
 */
sealed class SystemAction : BrowserAction() {
    /**
     * Optimizes the [BrowserState] by removing unneeded and optional
     * resources if the system is in a low memory condition.
     *
     * @param states map of session ids to engine session states where the engine session was closed
     * by SessionManager.
     */
    data class LowMemoryAction(
        val states: Map<String, EngineSessionState>
    ) : SystemAction()
}

/**
 * [BrowserAction] implementations related to updating the list of [TabSessionState] inside [BrowserState].
 */
sealed class TabListAction : BrowserAction() {
    /**
     * Adds a new [TabSessionState] to the [BrowserState.tabs] list.
     *
     * @property tab the [TabSessionState] to add
     * @property select whether or not to the tab should be selected.
     */
    data class AddTabAction(val tab: TabSessionState, val select: Boolean = false) : TabListAction()

    /**
     * Adds multiple [TabSessionState] objects to the [BrowserState.tabs] list.
     */
    data class AddMultipleTabsAction(val tabs: List<TabSessionState>) : TabListAction()

    /**
     * Marks the [TabSessionState] with the given [tabId] as selected tab.
     *
     * @property tabId the ID of the tab to select.
     */
    data class SelectTabAction(val tabId: String, val timeSelected: Long = System.currentTimeMillis()) : TabListAction()

    /**
     * Removes the [TabSessionState] with the given [tabId] from the list of sessions.
     *
     * @property tabId the ID of the tab to remove.
     * @property selectParentIfExists whether or not a parent tab should be
     * selected if one exists, defaults to true.
     */
    data class RemoveTabAction(val tabId: String, val selectParentIfExists: Boolean = true) : TabListAction()

    /**
     * Restores state from a (partial) previous state.
     *
     * @property tabs the [TabSessionState]s to restore.
     * @property selectedTabId the ID of the tab to select.
     */
    data class RestoreAction(val tabs: List<TabSessionState>, val selectedTabId: String? = null) : TabListAction()

    /**
     * Removes both private and normal [TabSessionState]s.
     */
    object RemoveAllTabsAction : TabListAction()

    /**
     * Removes all private [TabSessionState]s.
     */
    object RemoveAllPrivateTabsAction : TabListAction()

    /**
     * Removes all non-private [TabSessionState]s.
     */
    object RemoveAllNormalTabsAction : TabListAction()
}

/**
 * [BrowserAction] implementations related to updating [BrowserState.customTabs].
 */
sealed class CustomTabListAction : BrowserAction() {
    /**
     * Adds a new [CustomTabSessionState] to [BrowserState.customTabs].
     *
     * @property tab the [CustomTabSessionState] to add.
     */
    data class AddCustomTabAction(val tab: CustomTabSessionState) : CustomTabListAction()

    /**
     * Removes an existing [CustomTabSessionState] to [BrowserState.customTabs].
     *
     * @property tabId the ID of the custom tab to remove.
     */
    data class RemoveCustomTabAction(val tabId: String) : CustomTabListAction()

    /**
     * Removes all custom tabs [TabSessionState]s.
     */
    object RemoveAllCustomTabsAction : CustomTabListAction()
}

/**
 * [BrowserAction] implementations related to updating the [ContentState] of a single [SessionState] inside
 * [BrowserState].
 */
sealed class ContentAction : BrowserAction() {
    /**
     * Removes the icon of the [ContentState] with the given [sessionId].
     */
    data class RemoveIconAction(val sessionId: String) : ContentAction()

    /**
     * Removes the thumbnail of the [ContentState] with the given [sessionId].
     */
    data class RemoveThumbnailAction(val sessionId: String) : ContentAction()

    /**
     * Updates the URL of the [ContentState] with the given [sessionId].
     */
    data class UpdateUrlAction(val sessionId: String, val url: String) : ContentAction()

    /**
     * Updates the progress of the [ContentState] with the given [sessionId].
     */
    data class UpdateProgressAction(val sessionId: String, val progress: Int) : ContentAction()

    /**
     * Updates the title of the [ContentState] with the given [sessionId].
     */
    data class UpdateTitleAction(val sessionId: String, val title: String) : ContentAction()

    /**
     * Updates the loading state of the [ContentState] with the given [sessionId].
     */
    data class UpdateLoadingStateAction(val sessionId: String, val loading: Boolean) : ContentAction()

    /**
     * Updates the search terms of the [ContentState] with the given [sessionId].
     */
    data class UpdateSearchTermsAction(val sessionId: String, val searchTerms: String) : ContentAction()

    /**
     * Updates the [SecurityInfoState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateSecurityInfoAction(val sessionId: String, val securityInfo: SecurityInfoState) : ContentAction()

    /**
     * Updates the icon of the [ContentState] with the given [sessionId].
     */
    data class UpdateIconAction(val sessionId: String, val pageUrl: String, val icon: Bitmap) : ContentAction()

    /**
     * Updates the thumbnail of the [ContentState] with the given [sessionId].
     */
    data class UpdateThumbnailAction(val sessionId: String, val thumbnail: Bitmap) : ContentAction()

    /**
     * Updates the [DownloadState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateDownloadAction(val sessionId: String, val download: DownloadState) : ContentAction()

    /**
     * Removes the [DownloadState] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeDownloadAction(val sessionId: String, val downloadId: Long) : ContentAction()

    /**
     * Updates the [HitResult] of the [ContentState] with the given [sessionId].
     */
    data class UpdateHitResultAction(val sessionId: String, val hitResult: HitResult) : ContentAction()

    /**
     * Removes the [HitResult] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeHitResultAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [PromptRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdatePromptRequestAction(val sessionId: String, val promptRequest: PromptRequest) : ContentAction()

    /**
     * Removes the [PromptRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumePromptRequestAction(val sessionId: String) : ContentAction()

    /**
     * Adds a [FindResultState] to the [ContentState] with the given [sessionId].
     */
    data class AddFindResultAction(val sessionId: String, val findResult: FindResultState) : ContentAction()

    /**
     * Removes all [FindResultState]s of the [ContentState] with the given [sessionId].
     */
    data class ClearFindResultsAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [WindowRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateWindowRequestAction(val sessionId: String, val windowRequest: WindowRequest) : ContentAction()

    /**
     * Removes the [WindowRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeWindowRequestAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [SearchRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateSearchRequestAction(val sessionId: String, val searchRequest: SearchRequest) : ContentAction()

    /**
     * Removes the [SearchRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeSearchRequestAction(val sessionId: String) : ContentAction()

    /**
     * Updates [fullScreenEnabled] with the given [sessionId].
     */
    data class FullScreenChangedAction(val sessionId: String, val fullScreenEnabled: Boolean) : ContentAction()

    /**
     * Updates [pipEnabled] with the given [sessionId].
     */
    data class PictureInPictureChangedAction(val sessionId: String, val pipEnabled: Boolean) : ContentAction()

    /**
     * Updates the [layoutInDisplayCutoutMode] with the given [sessionId].
     *
     * @property sessionId the ID of the session
     * @property layoutInDisplayCutoutMode value of defined in https://developer.android.com/reference/android/view/WindowManager.LayoutParams#layoutInDisplayCutoutMode
     */
    data class ViewportFitChangedAction(val sessionId: String, val layoutInDisplayCutoutMode: Int) : ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate whether or not a back navigation is possible.
     */
    data class UpdateBackNavigationStateAction(val sessionId: String, val canGoBack: Boolean) : ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate whether the first contentful paint has happened.
     */
    data class UpdateFirstContentfulPaintStateAction(
        val sessionId: String,
        val firstContentfulPaint: Boolean
    ) : ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate whether or not a forward navigation is possible.
     */
    data class UpdateForwardNavigationStateAction(val sessionId: String, val canGoForward: Boolean) : ContentAction()

    /**
     * Updates the [WebAppManifest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateWebAppManifestAction(val sessionId: String, val webAppManifest: WebAppManifest) : ContentAction()

    /**
     * Removes the [WebAppManifest] of the [ContentState] with the given [sessionId].
     */
    data class RemoveWebAppManifestAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate the current history state.
     */
    data class UpdateHistoryStateAction(
        val sessionId: String,
        val historyList: List<HistoryItem>,
        val currentIndex: Int
    ) : ContentAction()
}

/**
 * [BrowserAction] implementations related to updating the [TrackingProtectionState] of a single [SessionState] inside
 * [BrowserState].
 */
sealed class TrackingProtectionAction : BrowserAction() {
    /**
     * Updates the [TrackingProtectionState.enabled] flag.
     */
    data class ToggleAction(val tabId: String, val enabled: Boolean) : TrackingProtectionAction()

    /**
     * Updates the [TrackingProtectionState.ignoredOnTrackingProtection] flag.
     */
    data class ToggleExclusionListAction(val tabId: String, val excluded: Boolean) : TrackingProtectionAction()

    /**
     * Adds a [Tracker] to the [TrackingProtectionState.blockedTrackers] list.
     */
    data class TrackerBlockedAction(val tabId: String, val tracker: Tracker) : TrackingProtectionAction()

    /**
     * Adds a [Tracker] to the [TrackingProtectionState.loadedTrackers] list.
     */
    data class TrackerLoadedAction(val tabId: String, val tracker: Tracker) : TrackingProtectionAction()

    /**
     * Clears the [TrackingProtectionState.blockedTrackers] and [TrackingProtectionState.blockedTrackers] lists.
     */
    data class ClearTrackersAction(val tabId: String) : TrackingProtectionAction()
}

/**
 * [BrowserAction] implementations related to updating [BrowserState.extensions] and
 * [TabSessionState.extensionState].
 */
sealed class WebExtensionAction : BrowserAction() {
    /**
     * Updates [BrowserState.extensions] to register the given [extension] as installed.
     */
    data class InstallWebExtensionAction(val extension: WebExtensionState) : WebExtensionAction()

    /**
     * Removes all state of the uninstalled extension from [BrowserState.extensions]
     * and [TabSessionState.extensionState].
     */
    data class UninstallWebExtensionAction(val extensionId: String) : WebExtensionAction()

    /**
     * Removes state of all extensions from [BrowserState.extensions]
     * and [TabSessionState.extensionState].
     */
    object UninstallAllWebExtensionsAction : WebExtensionAction()

    /**
     * Updates the [WebExtensionState.enabled] flag.
     */
    data class UpdateWebExtensionEnabledAction(val extensionId: String, val enabled: Boolean) :
        WebExtensionAction()

    /**
     * Updates the [WebExtensionState.enabled] flag.
     */
    data class UpdateWebExtensionAllowedInPrivateBrowsingAction(val extensionId: String, val allowed: Boolean) :
        WebExtensionAction()

    /**
     * Updates the given [updatedExtension] in the [BrowserState.extensions].
     */
    data class UpdateWebExtensionAction(val updatedExtension: WebExtensionState) : WebExtensionAction()

    /**
     * Updates a browser action of a given [extensionId].
     */
    data class UpdateBrowserAction(
        val extensionId: String,
        val browserAction: WebExtensionBrowserAction
    ) : WebExtensionAction()

    /**
     * Updates a page action of a given [extensionId].
     */
    data class UpdatePageAction(
        val extensionId: String,
        val pageAction: WebExtensionPageAction
    ) : WebExtensionAction()

    /**
     * Keeps track of the last session used to display an extension action popup.
     */
    data class UpdatePopupSessionAction(
        val extensionId: String,
        val popupSessionId: String? = null,
        val popupSession: EngineSession? = null
    ) : WebExtensionAction()

    /**
     * Updates a tab-specific browser action that belongs to the given [sessionId] and [extensionId] on the
     * [TabSessionState.extensionState].
     */
    data class UpdateTabBrowserAction(
        val sessionId: String,
        val extensionId: String,
        val browserAction: WebExtensionBrowserAction
    ) : WebExtensionAction()

    /**
     * Updates a page action that belongs to the given [sessionId] and [extensionId] on the
     * [TabSessionState.extensionState].
     */
    data class UpdateTabPageAction(
        val sessionId: String,
        val extensionId: String,
        val pageAction: WebExtensionPageAction
    ) : WebExtensionAction()
}

/**
 * [BrowserAction] implementations related to updating the [EngineState] of a single [SessionState] inside
 * [BrowserState].
 */
sealed class EngineAction : BrowserAction() {

    /**
     * Attaches the provided [EngineSession] to the session with the provided [sessionId].
     */
    data class LinkEngineSessionAction(val sessionId: String, val engineSession: EngineSession) : EngineAction()

    /**
     * Detaches the current [EngineSession] from the session with the provided [sessionId].
     */
    data class UnlinkEngineSessionAction(val sessionId: String) : EngineAction()

    /**
     * Updates the [EngineSessionState] of the session with the provided [sessionId].
     */
    data class UpdateEngineSessionStateAction(
        val sessionId: String,
        val engineSessionState: EngineSessionState
    ) : EngineAction()
}

/**
 * [BrowserAction] implementations related to updating the [ReaderState] of a single [TabSessionState] inside
 * [BrowserState].
 */
sealed class ReaderAction : BrowserAction() {
    /**
     * Updates the [ReaderState.readerable] flag.
     */
    data class UpdateReaderableAction(val tabId: String, val readerable: Boolean) : ReaderAction()

    /**
     * Updates the [ReaderState.active] flag.
     */
    data class UpdateReaderActiveAction(val tabId: String, val active: Boolean) : ReaderAction()

    /**
     * Updates the [ReaderState.checkRequired] flag.
     */
    data class UpdateReaderableCheckRequiredAction(val tabId: String, val checkRequired: Boolean) : ReaderAction()

    /**
     * Updates the [ReaderState.connectRequired] flag.
     */
    data class UpdateReaderConnectRequiredAction(val tabId: String, val connectRequired: Boolean) : ReaderAction()

    /**
    * Updates the [ReaderState.readerBaseUrl].
    */
    data class UpdateReaderBaseUrlAction(val tabId: String, val baseUrl: String) : ReaderAction()

    /**
     * Updates the [ReaderState.activeUrl].
     */
    data class UpdateReaderActiveUrlAction(val tabId: String, val activeUrl: String) : ReaderAction()

    /**
     * Clears the [ReaderState.activeUrl].
     */
    data class ClearReaderActiveUrlAction(val tabId: String) : ReaderAction()
}

/**
 * [BrowserAction] implementations related to updating the [MediaState].
 */
sealed class MediaAction : BrowserAction() {
    /**
     * Adds [media] to the list of [MediaState.Element] for the tab with id [tabId].
     */
    data class AddMediaAction(val tabId: String, val media: MediaState.Element) : MediaAction()

    /**
     * Removes [media] from the list of [MediaState.Element] for the tab with id [tabId].
     */
    data class RemoveMediaAction(val tabId: String, val media: MediaState.Element) : MediaAction()

    /**
     * Removes all [MediaState.Element] for tabs with ids in [tabIds].
     */
    data class RemoveTabMediaAction(val tabIds: List<String>) : MediaAction()

    /**
     * Updates the [Media.State] for the [MediaState.Element] with id [mediaId] owned by the tab
     * with id [tabId].
     */
    data class UpdateMediaStateAction(
        val tabId: String,
        val mediaId: String,
        val state: Media.State
    ) : MediaAction()

    /**
     * Updates the [Media.PlaybackState] for the [MediaState.Element] with id [mediaId] owned by the
     * tab with id [tabId].
     */
    data class UpdateMediaPlaybackStateAction(
        val tabId: String,
        val mediaId: String,
        val playbackState: Media.PlaybackState
    ) : MediaAction()

    /**
     * Updates the [Media.Metadata] for the [MediaState.Element] with id [mediaId] owned by the tab
     * with id [tabId].
     */
    data class UpdateMediaMetadataAction(
        val tabId: String,
        val mediaId: String,
        val metadata: Media.Metadata
    ) : MediaAction()

    /**
     * Updates the [Media.Volume] for the [MediaState.Element] with id [mediaId] owned by the tab
     * with id [tabId].
     */
    data class UpdateMediaVolumeAction(
        val tabId: String,
        val mediaId: String,
        val volume: Media.Volume
    ) : MediaAction()

    /**
     * Updates the [Media.fullscreen] for the [MediaState.Element] with id [mediaId] owned by the tab
     * with id [tabId].
     */
    data class UpdateMediaFullscreenAction(
        val tabId: String,
        val mediaId: String,
        val fullScreen: Boolean
    ) : MediaAction()

    /**
     * Updates [MediaState.Aggregate] in the [MediaState].
     */
    data class UpdateMediaAggregateAction(
        val aggregate: MediaState.Aggregate
    ) : MediaAction()
}

/**
 * [BrowserAction] implementations related to updating the global download state.
 */
sealed class DownloadAction : BrowserAction() {
    /**
     * Updates the [BrowserState] to track the provided [download] as added.
     */
    data class AddDownloadAction(val download: DownloadState) : DownloadAction()

    /**
     * Updates the [BrowserState] to remove the download with the provided [downloadId].
     */
    data class RemoveDownloadAction(val downloadId: Long) : DownloadAction()

    /**
     * Updates the [BrowserState] to remove all downloads.
     */
    object RemoveAllDownloadsAction : DownloadAction()

    /**
     * Updates the provided [download] on the [BrowserState].
     */
    data class UpdateDownloadAction(val download: DownloadState) : DownloadAction()
}

/**
 * [BrowserAction] implementations related to updating [BrowserState.containers]
 */
sealed class ContainerAction : BrowserAction() {
    /**
     * Updates [BrowserState.containers] to register the given added [container].
     */
    data class AddContainerAction(val container: ContainerState) : ContainerAction()

    /**
     * Updates [BrowserState.containers] to register the given list of [containers].
     */
    data class AddContainersAction(val containers: List<ContainerState>) : ContainerAction()

    /**
     * Initializes the [BrowserState.containers] state.
     */
    object InitializeContainerState : ContainerAction()

    /**
     * Removes all state of the removed container from [BrowserState.containers].
     */
    data class RemoveContainerAction(val contextId: String) : ContainerAction()
}

/**
 * [BrowserAction] implementations related to updating search engines in [SearchState].
 */
sealed class SearchAction : BrowserAction() {
    /**
     * Updates [BrowserState.search] to add/modify [SearchState.searchEngines].
     */
    data class AddSearchEngineListAction(val searchEngineList: List<SearchEngine>) : SearchAction()

    /**
     * Updates [BrowserState.search] to add/modify a custom [SearchEngine].
     */
    data class SetCustomSearchEngineAction(val searchEngine: SearchEngine) : SearchAction()

    /**
     * Updates [BrowserState.search] to remove a custom [SearchEngine].
     */
    data class RemoveCustomSearchEngineAction(val searchEngineId: String) : SearchAction()

    /**
     * Updates [BrowserState.search] to update [SearchState.defaultSearchEngineId].
     */
    data class SetDefaultSearchEngineAction(val searchEngineId: String) : SearchAction()
}
