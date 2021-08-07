/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import android.content.ComponentCallbacks2
import android.graphics.Bitmap
import android.os.SystemClock
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContainerState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.LoadRequestState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.UndoHistoryState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
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
import mozilla.components.concept.engine.search.SearchRequest
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.DelicateAction
import mozilla.components.support.base.android.Clock
import java.util.Locale

/**
 * [Action] implementation related to [BrowserState].
 */
sealed class BrowserAction : Action

/**
 * [BrowserAction] dispatched to indicate that the store is initialized and
 * ready to use. This action is dispatched automatically before any other
 * action is processed. Its main purpose is to trigger initialization logic
 * in middlewares. The action itself has no effect on the [BrowserState].
 */
object InitAction : BrowserAction()

/**
 * [BrowserAction] to indicate that restoring [BrowserState] is complete.
 */
object RestoreCompleteAction : BrowserAction()

/**
 * [BrowserAction] implementations to react to system events.
 */
sealed class SystemAction : BrowserAction() {
    /**
     * Optimizes the [BrowserState] by removing unneeded and optional resources if the system is in
     * a low memory condition.
     *
     * @param level The context of the trim, giving a hint of the amount of trimming the application
     * may like to perform. See constants in [ComponentCallbacks2].
     */
    data class LowMemoryAction(
        val level: Int
    ) : SystemAction()
}

/**
 * [BrowserAction] implementations related to updating the [Locale] inside [BrowserState].
 */
sealed class LocaleAction : BrowserAction() {
    /**
     * Updating the [BrowserState] to reflect app [Locale] changes.
     *
     * @property locale the updated [Locale]
     */
    data class UpdateLocaleAction(val locale: Locale?) : LocaleAction()

    /**
     * Restores the [BrowserState.locale] state from storage.
     */
    object RestoreLocaleStateAction : LocaleAction()
}

/**
 * [BrowserAction] implementations related to updating the list of [ClosedTabSessionState] inside [BrowserState].
 */
sealed class RecentlyClosedAction : BrowserAction() {
    /**
     * Adds a list of [RecoverableTab] to the [BrowserState.closedTabs] list.
     *
     * @property tabs the [RecoverableTab]s to add
     */
    data class AddClosedTabsAction(val tabs: List<RecoverableTab>) : RecentlyClosedAction()

    /**
     * Removes a [RecoverableTab] from the [BrowserState.closedTabs] list.
     *
     * @property tab the [RecoverableTab] to remove
     */
    data class RemoveClosedTabAction(val tab: RecoverableTab) : RecentlyClosedAction()

    /**
     * Removes all [RecoverableTab]s from the [BrowserState.closedTabs] list.
     */
    object RemoveAllClosedTabAction : RecentlyClosedAction()

    /**
     * Prunes [RecoverableTab]s from the [BrowserState.closedTabs] list to keep only [maxTabs].
     */
    data class PruneClosedTabsAction(val maxTabs: Int) : RecentlyClosedAction()

    /**
     * Updates [BrowserState.closedTabs] to register the given list of [ClosedTab].
     */
    data class ReplaceTabsAction(val tabs: List<RecoverableTab>) : RecentlyClosedAction()
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
    data class SelectTabAction(val tabId: String) : TabListAction()

    /**
     * Removes the [TabSessionState] with the given [tabId] from the list of sessions.
     *
     * @property tabId the ID of the tab to remove.
     * @property selectParentIfExists whether or not a parent tab should be
     * selected if one exists, defaults to true.
     */
    data class RemoveTabAction(val tabId: String, val selectParentIfExists: Boolean = true) :
        TabListAction()

    /**
     * Removes the [TabSessionState]s with the given [tabId]s from the list of sessions.
     *
     * @property tabIds the IDs of the tabs to remove.
     */
    data class RemoveTabsAction(val tabIds: List<String>) : TabListAction()

    /**
     * Restores state from a (partial) previous state.
     *
     * @property tabs the [TabSessionState]s to restore.
     * @property selectedTabId the ID of the tab to select.
     */
    data class RestoreAction(val tabs: List<TabSessionState>, val selectedTabId: String? = null) :
        TabListAction()

    /**
     * Removes both private and normal [TabSessionState]s.
     * @property recoverable Indicates whether removed tabs should be recoverable.
     */
    data class RemoveAllTabsAction(val recoverable: Boolean = true) : TabListAction()

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
 * [BrowserAction] implementations dealing with "undo" after removing a tab.
 */
sealed class UndoAction : BrowserAction() {
    /**
     * Adds the list of [tabs] to [UndoHistoryState] with the given [tag].
     */
    data class AddRecoverableTabs(
        val tag: String,
        val tabs: List<RecoverableTab>,
        val selectedTabId: String?
    ) : UndoAction()

    /**
     * Clears the tabs from [UndoHistoryState] for the given [tag].
     */
    data class ClearRecoverableTabs(
        val tag: String
    ) : UndoAction()

    /**
     * Restores the tabs in [UndoHistoryState].
     */
    object RestoreRecoverableTabs : UndoAction()
}

/**
 * [BrowserAction] implementations related to updating the [TabSessionState] inside [BrowserState].
 */
sealed class LastAccessAction : BrowserAction() {
    /**
     * Updates the [TabSessionState.lastAccess] timestamp of the tab with the given [tabId].
     *
     * @property tabId the ID of the tab to update.
     * @property lastAccess the value to signify when the tab was last accessed; defaults to [System.currentTimeMillis].
     */
    data class UpdateLastAccessAction(
        val tabId: String,
        val lastAccess: Long = System.currentTimeMillis()
    ) : LastAccessAction()

    /**
     * Updates [TabSessionState.lastMediaAccessState] for when media started playing in the tab identified by [tabId].
     *
     * @property tabId the ID of the tab to update.
     * @property lastMediaAccess the value to signify when the tab last started playing media.
     * Defaults to [System.currentTimeMillis].
     */
    data class UpdateLastMediaAccessAction(
        val tabId: String,
        val lastMediaAccess: Long = System.currentTimeMillis()
    ) : LastAccessAction()

    /**
     * Updates [TabSessionState.lastMediaAccessState] when the media session of this tab is deactivated.
     *
     * @property tabId the ID of the tab to update.
     */
    data class ResetLastMediaSessionAction(
        val tabId: String
    ) : LastAccessAction()
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
     * Converts an existing [CustomTabSessionState] to a regular/normal [TabSessionState].
     */
    data class TurnCustomTabIntoNormalTabAction(val tabId: String) : CustomTabListAction()

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
     * Updates permissions highlights of the [ContentState] with the given [sessionId].
     */
    sealed class UpdatePermissionHighlightsStateAction : ContentAction() {
        /**
         * Updates the notificationChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class NotificationChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the cameraChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class CameraChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the locationChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class LocationChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the microphoneChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class MicrophoneChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the persistentStorageChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class PersistentStorageChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the mediaKeySystemAccessChanged property of the [PermissionHighlightsState]
         * with the given [tabId].
         */
        data class MediaKeySystemAccesChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the autoPlayAudibleChanged property of the [PermissionHighlightsState]
         * with the given [tabId].
         */
        data class AutoPlayAudibleChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the autoPlayInaudibleChanged property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class AutoPlayInAudibleChangedAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the autoPlayAudibleBlocking property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class AutoPlayAudibleBlockingAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates the autoPlayInaudibleBlocking property of the [PermissionHighlightsState] with the given [tabId].
         */
        data class AutoPlayInAudibleBlockingAction(val tabId: String, val value: Boolean) :
            UpdatePermissionHighlightsStateAction()

        /**
         * Updates permissions highlights of the [ContentState] with the given [tabId]
         * to its default value.
         */
        data class Reset(val tabId: String) : UpdatePermissionHighlightsStateAction()
    }
    /**
     * Updates the title of the [ContentState] with the given [sessionId].
     */
    data class UpdateTitleAction(val sessionId: String, val title: String) : ContentAction()

    /**
     * Updates the loading state of the [ContentState] with the given [sessionId].
     */
    data class UpdateLoadingStateAction(val sessionId: String, val loading: Boolean) :
        ContentAction()

    /**
     * Updates the refreshCanceled state of the [ContentState] with the given [sessionId].
     */
    data class UpdateRefreshCanceledStateAction(val sessionId: String, val refreshCanceled: Boolean) :
        ContentAction()

    /**
     * Updates the search terms of the [ContentState] with the given [sessionId].
     */
    data class UpdateSearchTermsAction(val sessionId: String, val searchTerms: String) :
        ContentAction()

    /**
     * Updates the [SecurityInfoState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateSecurityInfoAction(
        val sessionId: String,
        val securityInfo: SecurityInfoState
    ) : ContentAction()

    /**
     * Updates the icon of the [ContentState] with the given [sessionId].
     */
    data class UpdateIconAction(val sessionId: String, val pageUrl: String, val icon: Bitmap) :
        ContentAction()

    /**
     * Updates the thumbnail of the [ContentState] with the given [sessionId].
     */
    data class UpdateThumbnailAction(val sessionId: String, val thumbnail: Bitmap) : ContentAction()

    /**
     * Updates the [DownloadState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateDownloadAction(val sessionId: String, val download: DownloadState) :
        ContentAction()

    /**
     * Closes the [DownloadState.response] of the [ContentState.download]
     * and removes the [DownloadState] of the [ContentState] with the given [sessionId].
     */
    data class CancelDownloadAction(val sessionId: String, val downloadId: String) : ContentAction()

    /**
     * Removes the [DownloadState] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeDownloadAction(val sessionId: String, val downloadId: String) : ContentAction()
    /**
     * Updates the [HitResult] of the [ContentState] with the given [sessionId].
     */
    data class UpdateHitResultAction(val sessionId: String, val hitResult: HitResult) :
        ContentAction()

    /**
     * Removes the [HitResult] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeHitResultAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [PromptRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdatePromptRequestAction(val sessionId: String, val promptRequest: PromptRequest) :
        ContentAction()

    /**
     * Removes the [PromptRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumePromptRequestAction(val sessionId: String, val promptRequest: PromptRequest) :
        ContentAction()

    /**
     * Adds a [FindResultState] to the [ContentState] with the given [sessionId].
     */
    data class AddFindResultAction(val sessionId: String, val findResult: FindResultState) :
        ContentAction()

    /**
     * Removes all [FindResultState]s of the [ContentState] with the given [sessionId].
     */
    data class ClearFindResultsAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [WindowRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateWindowRequestAction(val sessionId: String, val windowRequest: WindowRequest) :
        ContentAction()

    /**
     * Removes the [WindowRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeWindowRequestAction(val sessionId: String) : ContentAction()

    /**
     * Updates the [SearchRequest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateSearchRequestAction(val sessionId: String, val searchRequest: SearchRequest) :
        ContentAction()

    /**
     * Removes the [SearchRequest] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeSearchRequestAction(val sessionId: String) : ContentAction()

    /**
     * Updates [fullScreenEnabled] with the given [sessionId].
     */
    data class FullScreenChangedAction(val sessionId: String, val fullScreenEnabled: Boolean) :
        ContentAction()

    /**
     * Updates [pipEnabled] with the given [sessionId].
     */
    data class PictureInPictureChangedAction(val sessionId: String, val pipEnabled: Boolean) :
        ContentAction()

    /**
     * Updates the [layoutInDisplayCutoutMode] with the given [sessionId].
     *
     * @property sessionId the ID of the session
     * @property layoutInDisplayCutoutMode value of defined in https://developer.android.com/reference/android/view/WindowManager.LayoutParams#layoutInDisplayCutoutMode
     */
    data class ViewportFitChangedAction(val sessionId: String, val layoutInDisplayCutoutMode: Int) :
        ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate whether or not a back navigation is possible.
     */
    data class UpdateBackNavigationStateAction(val sessionId: String, val canGoBack: Boolean) :
        ContentAction()

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
    data class UpdateForwardNavigationStateAction(
        val sessionId: String,
        val canGoForward: Boolean
    ) : ContentAction()

    /**
     * Updates the [WebAppManifest] of the [ContentState] with the given [sessionId].
     */
    data class UpdateWebAppManifestAction(
        val sessionId: String,
        val webAppManifest: WebAppManifest
    ) : ContentAction()

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

    /**
     * Updates the [LoadRequestState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateLoadRequestAction(val sessionId: String, val loadRequest: LoadRequestState) : ContentAction()

    /**
     * Adds a new content permission request to the [ContentState] list.
     * */
    data class UpdatePermissionsRequest(
        val sessionId: String,
        val permissionRequest: PermissionRequest
    ) : ContentAction()

    /**
     * Deletes a content permission request from the [ContentState] list.
     * */
    data class ConsumePermissionsRequest(
        val sessionId: String,
        val permissionRequest: PermissionRequest
    ) : ContentAction()

    /**
     * Removes all content permission requests from the [ContentState] list.
     * */
    data class ClearPermissionRequests(
        val sessionId: String
    ) : ContentAction()

    /**
     * Adds a new app permission request to the [ContentState] list.
     * */
    data class UpdateAppPermissionsRequest(
        val sessionId: String,
        val appPermissionRequest: PermissionRequest
    ) : ContentAction()

    /**
     * Deletes an app permission request from the [ContentState] list.
     * */
    data class ConsumeAppPermissionsRequest(
        val sessionId: String,
        val appPermissionRequest: PermissionRequest
    ) : ContentAction()

    /**
     * Removes all app permission requests from the [ContentState] list.
     * */
    data class ClearAppPermissionRequests(
        val sessionId: String
    ) : ContentAction()

    /**
     * Sets the list of active recording devices (webcam, microphone, ..) used by web content.
     */
    data class SetRecordingDevices(
        val sessionId: String,
        val devices: List<RecordingDevice>
    ) : ContentAction()

    /**
     * Updates the [ContentState] of the given [sessionId] to indicate whether or not desktop mode is enabled.
     */
    data class UpdateDesktopModeAction(val sessionId: String, val enabled: Boolean) : ContentAction()

    /**
     * Updates the [AppIntentState] of the [ContentState] with the given [sessionId].
     */
    data class UpdateAppIntentAction(val sessionId: String, val appIntent: AppIntentState) :
        ContentAction()

    /**
     * Removes the [AppIntentState] of the [ContentState] with the given [sessionId].
     */
    data class ConsumeAppIntentAction(val sessionId: String) : ContentAction()
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
    data class ToggleExclusionListAction(val tabId: String, val excluded: Boolean) :
        TrackingProtectionAction()

    /**
     * Adds a [Tracker] to the [TrackingProtectionState.blockedTrackers] list.
     */
    data class TrackerBlockedAction(val tabId: String, val tracker: Tracker) :
        TrackingProtectionAction()

    /**
     * Adds a [Tracker] to the [TrackingProtectionState.loadedTrackers] list.
     */
    data class TrackerLoadedAction(val tabId: String, val tracker: Tracker) :
        TrackingProtectionAction()

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
     * Updates the [WebExtensionState.allowedInPrivateBrowsing] flag.
     */
    data class UpdateWebExtensionAllowedInPrivateBrowsingAction(
        val extensionId: String,
        val allowed: Boolean
    ) :
        WebExtensionAction()

    /**
     * Updates the given [updatedExtension] in the [BrowserState.extensions].
     */
    data class UpdateWebExtensionAction(val updatedExtension: WebExtensionState) :
        WebExtensionAction()

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

    /**
     * Updates the [BrowserState.activeWebExtensionTabId] to mark a tab active for web extensions
     * e.g. to support tabs.query({active: true}).
     */
    data class UpdateActiveWebExtensionTabAction(
        val activeWebExtensionTabId: String?
    ) : WebExtensionAction()
}

/**
 * [BrowserAction] implementations related to updating the [EngineState] of a single [SessionState] inside
 * [BrowserState].
 */
sealed class EngineAction : BrowserAction() {
    /**
     * Creates an [EngineSession] for the given [tabId] if none exists yet.
     */
    data class CreateEngineSessionAction(
        override val tabId: String,
        val skipLoading: Boolean = false,
        val followupAction: BrowserAction? = null
    ) : EngineAction(), ActionWithTab

    /**
     * Loads the given [url] in the tab with the given [tabId].
     */
    data class LoadUrlAction(
        override val tabId: String,
        val url: String,
        val flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
        val additionalHeaders: Map<String, String>? = null
    ) : EngineAction(), ActionWithTab

    /**
     * Loads [data] in the tab with the given [tabId].
     */
    data class LoadDataAction(
        override val tabId: String,
        val data: String,
        val mimeType: String = "text/html",
        val encoding: String = "UTF-8"
    ) : EngineAction(), ActionWithTab

    /**
     * Reloads the tab with the given [tabId].
     */
    data class ReloadAction(
        override val tabId: String,
        val flags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none()
    ) : EngineAction(), ActionWithTab

    /**
     * Navigates back in the tab with the given [tabId].
     */
    data class GoBackAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Navigates forward in the tab with the given [tabId].
     */
    data class GoForwardAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Navigates to the specified index in the history of the tab with the given [tabId].
     */
    data class GoToHistoryIndexAction(
        override val tabId: String,
        val index: Int
    ) : EngineAction(), ActionWithTab

    /**
     * Enables/disables desktop mode in the tabs with the given [tabId].
     */
    data class ToggleDesktopModeAction(
        override val tabId: String,
        val enable: Boolean
    ) : EngineAction(), ActionWithTab

    /**
     * Exits fullscreen mode in the tabs with the given [tabId].
     */
    data class ExitFullScreenModeAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Clears browsing data for the tab with the given [tabId].
     */
    data class ClearDataAction(
        override val tabId: String,
        val data: Engine.BrowsingData
    ) : EngineAction(), ActionWithTab

    /**
     * Attaches the provided [EngineSession] to the session with the provided [tabId].
     *
     * @property tabId The ID of the tab the [EngineSession] should be linked to.
     * @property engineSession The [EngineSession] that should be linked to the tab.
     * @property timestamp Timestamp (milliseconds) of when the linking has happened (By default
     * set to [SystemClock.elapsedRealtime].
     */
    data class LinkEngineSessionAction(
        override val tabId: String,
        val engineSession: EngineSession,
        val timestamp: Long = Clock.elapsedRealtime(),
        val skipLoading: Boolean = false
    ) : EngineAction(), ActionWithTab

    /**
     * Suspends the [EngineSession] of the session with the provided [tabId].
     */
    data class SuspendEngineSessionAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Marks the [EngineSession] of the session with the provided [tabId] as killed (The matching
     * content process was killed).
     */
    data class KillEngineSessionAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Detaches the current [EngineSession] from the session with the provided [tabId].
     */
    data class UnlinkEngineSessionAction(
        override val tabId: String
    ) : EngineAction(), ActionWithTab

    /**
     * Updates the [EngineState.initializing] flag of the session with the provided [tabId].
     */
    data class UpdateEngineSessionInitializingAction(
        override val tabId: String,
        val initializing: Boolean
    ) : EngineAction(), ActionWithTab

    /**
     * Updates the [EngineSessionState] of the session with the provided [tabId].
     */
    data class UpdateEngineSessionStateAction(
        override val tabId: String,
        val engineSessionState: EngineSessionState
    ) : EngineAction(), ActionWithTab

    /**
     * Updates the [EngineSession.Observer] of the session with the provided [tabId].
     */
    data class UpdateEngineSessionObserverAction(
        override val tabId: String,
        val engineSessionObserver: EngineSession.Observer
    ) : EngineAction(), ActionWithTab

    /**
     * Purges the back/forward history of all tabs and custom tabs.
     */
    object PurgeHistoryAction : EngineAction()
}

/**
 * [BrowserAction] implementations to react to crashes.
 */
sealed class CrashAction : BrowserAction() {
    /**
     * Updates the [SessionState] of the session with provided ID to mark it as crashed.
     */
    data class SessionCrashedAction(val tabId: String) : CrashAction()

    /**
     * Updates the [SessionState] of the session with provided ID to mark it as restored.
     */
    data class RestoreCrashedSessionAction(val tabId: String) : CrashAction()
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
    data class UpdateReaderableCheckRequiredAction(val tabId: String, val checkRequired: Boolean) :
        ReaderAction()

    /**
     * Updates the [ReaderState.connectRequired] flag.
     */
    data class UpdateReaderConnectRequiredAction(val tabId: String, val connectRequired: Boolean) :
        ReaderAction()

    /**
     * Updates the [ReaderState.baseUrl].
     */
    data class UpdateReaderBaseUrlAction(val tabId: String, val baseUrl: String) : ReaderAction()

    /**
     * Updates the [ReaderState.activeUrl].
     */
    data class UpdateReaderActiveUrlAction(val tabId: String, val activeUrl: String) :
        ReaderAction()

    /**
     * Clears the [ReaderState.activeUrl].
     */
    data class ClearReaderActiveUrlAction(val tabId: String) : ReaderAction()
}

/**
 * [BrowserAction] implementations related to updating the [MediaSessionState].
 */
sealed class MediaSessionAction : BrowserAction() {
    /**
     * Activates [MediaSession] owned by the tab with id [tabId].
     */
    data class ActivatedMediaSessionAction(
        val tabId: String,
        val mediaSessionController: MediaSession.Controller
    ) : MediaSessionAction()

    /**
     * Activates [MediaSession] owned by the tab with id [tabId].
     */
    data class DeactivatedMediaSessionAction(
        val tabId: String
    ) : MediaSessionAction()

    /**
     * Updates the [MediaSession.Metadata] owned by the tab with id [tabId].
     */
    data class UpdateMediaMetadataAction(
        val tabId: String,
        val metadata: MediaSession.Metadata
    ) : MediaSessionAction()

    /**
     * Updates the [MediaSession.PlaybackState] owned by the tab with id [tabId].
     */
    data class UpdateMediaPlaybackStateAction(
        val tabId: String,
        val playbackState: MediaSession.PlaybackState
    ) : MediaSessionAction()

    /**
     * Updates the [MediaSession.Feature] owned by the tab with id [tabId].
     */
    data class UpdateMediaFeatureAction(
        val tabId: String,
        val features: MediaSession.Feature
    ) : MediaSessionAction()

    /**
     * Updates the [MediaSession.PositionState] owned by the tab with id [tabId].
     */
    data class UpdateMediaPositionStateAction(
        val tabId: String,
        val positionState: MediaSession.PositionState
    ) : MediaSessionAction()

    /**
     * Updates the [muted] owned by the tab with id [tabId].
     */
    data class UpdateMediaMutedAction(
        val tabId: String,
        val muted: Boolean
    ) : MediaSessionAction()

    /**
     * Updates the [fullScreen] and [MediaSession.ElementMetadata] owned by the tab with id [tabId].
     */
    data class UpdateMediaFullscreenAction(
        val tabId: String,
        val fullScreen: Boolean,
        val elementMetadata: MediaSession.ElementMetadata?
    ) : MediaSessionAction()
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
    data class RemoveDownloadAction(val downloadId: String) : DownloadAction()

    /**
     * Updates the [BrowserState] to remove all downloads.
     */
    object RemoveAllDownloadsAction : DownloadAction()

    /**
     * Updates the provided [download] on the [BrowserState].
     */
    data class UpdateDownloadAction(val download: DownloadState) : DownloadAction()

    /**
     * Mark the download notification of the provided [downloadId] as removed from the status bar.
     */
    data class DismissDownloadNotificationAction(val downloadId: String) : DownloadAction()

    /**
     * Restores the [BrowserState.downloads] state from the storage.
     */
    object RestoreDownloadsStateAction : DownloadAction()

    /**
     * Restores the given [download] from the storage.
     */
    data class RestoreDownloadStateAction(val download: DownloadState) : DownloadAction()
}

/**
 * [BrowserAction] implementations related to updating the session state of internet resources to be shared.
 */
sealed class ShareInternetResourceAction : BrowserAction() {
    /**
     * Starts the sharing process of an Internet resource.
     */
    data class AddShareAction(
        val tabId: String,
        val internetResource: ShareInternetResourceState
    ) : ShareInternetResourceAction()

    /**
     * Previous share request is considered completed.
     * File was successfully shared with other apps / user may have aborted the process or the operation
     * may have failed. In either case the previous share request is considered completed.
     */
    data class ConsumeShareAction(
        val tabId: String
    ) : ShareInternetResourceAction()
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
     * Removes all state of the removed container from [BrowserState.containers].
     */
    data class RemoveContainerAction(val contextId: String) : ContainerAction()
}

/**
 * [BrowserAction] implementations related to updating [TabSessionState.historyMetadata].
 */
sealed class HistoryMetadataAction : BrowserAction() {

    /**
     * Associates a tab with a history metadata record described by the provided [historyMetadataKey].
     */
    data class SetHistoryMetadataKeyAction(
        val tabId: String,
        val historyMetadataKey: HistoryMetadataKey
    ) : HistoryMetadataAction()
}

/**
 * [BrowserAction] implementations related to updating search engines in [SearchState].
 */
sealed class SearchAction : BrowserAction() {
    /**
     * Sets the [RegionState] (region of the user).
     */
    data class SetRegionAction(val regionState: RegionState) : SearchAction()

    /**
     * Sets the list of search engines and default search engine IDs.
     */
    data class SetSearchEnginesAction(
        val regionSearchEngines: List<SearchEngine>,
        val customSearchEngines: List<SearchEngine>,
        val hiddenSearchEngines: List<SearchEngine>,
        val additionalSearchEngines: List<SearchEngine>,
        val additionalAvailableSearchEngines: List<SearchEngine>,
        val userSelectedSearchEngineId: String?,
        val userSelectedSearchEngineName: String?,
        val regionDefaultSearchEngineId: String,
        val regionSearchEnginesOrder: List<String>
    ) : SearchAction()

    /**
     * Updates [BrowserState.search] to add/modify a custom [SearchEngine].
     */
    data class UpdateCustomSearchEngineAction(val searchEngine: SearchEngine) : SearchAction()

    /**
     * Updates [BrowserState.search] to remove a custom [SearchEngine].
     */
    data class RemoveCustomSearchEngineAction(val searchEngineId: String) : SearchAction()

    /**
     * Updates [BrowserState.search] to update [SearchState.userSelectedSearchEngineId] and
     * [SearchState.userSelectedSearchEngineName].
     */
    data class SelectSearchEngineAction(
        val searchEngineId: String,
        val searchEngineName: String?
    ) : SearchAction()

    /**
     * Shows a previously hidden, bundled search engine in [SearchState.regionSearchEngines] again
     * and removes it from [SearchState.hiddenSearchEngines].
     */
    data class ShowSearchEngineAction(val searchEngineId: String) : SearchAction()

    /**
     * Hides a bundled search engine in [SearchState.regionSearchEngines] and adds it to
     * [SearchState.hiddenSearchEngines] instead.
     */
    data class HideSearchEngineAction(val searchEngineId: String) : SearchAction()

    /**
     * Adds an additional search engine from [SearchState.additionalAvailableSearchEngines] to
     * [SearchState.additionalSearchEngines].
     */
    data class AddAdditionalSearchEngineAction(val searchEngineId: String) : SearchAction()

    /**
     * Removes and additional search engine from [SearchState.additionalSearchEngines] and adds it
     * back to [SearchState.additionalAvailableSearchEngines].
     */
    data class RemoveAdditionalSearchEngineAction(val searchEngineId: String) : SearchAction()
}

/**
 * [BrowserAction] implementations for updating state needed for debugging. These actions should
 * be carefully considered before being used.
 *
 * Every action **should** be annotated with [DelicateAction] to bring consumers to attention that
 * this is a delicate action.
 */
sealed class DebugAction : BrowserAction() {

    /**
     * Updates the [TabSessionState.createdAt] timestamp of the tab with the given [tabId].
     *
     * @property tabId the ID of the tab to update.
     * @property createdAt the value to signify when the tab was created.
     */
    @DelicateAction
    data class UpdateCreatedAtAction(val tabId: String, val createdAt: Long) : DebugAction()
}
