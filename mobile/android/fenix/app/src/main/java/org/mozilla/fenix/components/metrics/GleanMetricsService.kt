/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.private.NoExtraKeys
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.AboutPage
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.AndroidAutofill
import org.mozilla.fenix.GleanMetrics.AndroidKeystoreExperiment
import org.mozilla.fenix.GleanMetrics.AppTheme
import org.mozilla.fenix.GleanMetrics.Autoplay
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.BannerOpenInApp
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.GleanMetrics.BrowserSearch
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.ContextualMenu
import org.mozilla.fenix.GleanMetrics.CrashReporter
import org.mozilla.fenix.GleanMetrics.CustomTab
import org.mozilla.fenix.GleanMetrics.DownloadNotification
import org.mozilla.fenix.GleanMetrics.DownloadsMisc
import org.mozilla.fenix.GleanMetrics.DownloadsManagement
import org.mozilla.fenix.GleanMetrics.ErrorPage
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.ExperimentsDefaultBrowser
import org.mozilla.fenix.GleanMetrics.FindInPage
import org.mozilla.fenix.GleanMetrics.History
import org.mozilla.fenix.GleanMetrics.HomeMenu
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.LoginDialog
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.GleanMetrics.MasterPassword
import org.mozilla.fenix.GleanMetrics.MediaNotification
import org.mozilla.fenix.GleanMetrics.MediaState
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingMode
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingShortcut
import org.mozilla.fenix.GleanMetrics.ProgressiveWebApp
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.GleanMetrics.RecentTabs
import org.mozilla.fenix.GleanMetrics.SearchShortcuts
import org.mozilla.fenix.GleanMetrics.SearchSuggestions
import org.mozilla.fenix.GleanMetrics.SearchWidget
import org.mozilla.fenix.GleanMetrics.SetDefaultNewtabExperiment
import org.mozilla.fenix.GleanMetrics.SetDefaultSettingExperiment
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.GleanMetrics.SyncAccount
import org.mozilla.fenix.GleanMetrics.SyncAuth
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.GleanMetrics.Tab
import org.mozilla.fenix.GleanMetrics.Tabs
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.GleanMetrics.Tip
import org.mozilla.fenix.GleanMetrics.ToolbarSettings
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.GleanMetrics.UserSpecifiedSearchEngines
import org.mozilla.fenix.GleanMetrics.VoiceSearch
import org.mozilla.fenix.ext.components

private class EventWrapper<T : Enum<T>>(
    private val recorder: ((Map<T, String>?) -> Unit),
    private val keyMapper: ((String) -> T)? = null
) {

    /**
     * Converts snake_case string to camelCase.
     */
    private fun String.asCamelCase(): String {
        val parts = split("_")
        val builder = StringBuilder()

        for ((index, part) in parts.withIndex()) {
            if (index == 0) {
                builder.append(part)
            } else {
                builder.append(part[0].uppercase())
                builder.append(part.substring(1))
            }
        }

        return builder.toString()
    }

    fun track(event: Event) {
        val extras = if (keyMapper != null) {
            event.extras?.mapKeys { (key) ->
                keyMapper.invoke(key.toString().asCamelCase())
            }
        } else {
            null
        }

        @Suppress("DEPRECATION")
        // FIXME(#19967): Migrate to non-deprecated API.
        this.recorder(extras)
    }
}

@Suppress("DEPRECATION")
// FIXME(#19967): Migrate to non-deprecated API.
private val Event.wrapper: EventWrapper<*>?
    get() = when (this) {
        is Event.OpenedApp -> EventWrapper(
            { Events.appOpened.record(it) },
            { Events.appOpenedKeys.valueOf(it) }
        )
        is Event.SearchBarTapped -> EventWrapper(
            { Events.searchBarTapped.record(it) },
            { Events.searchBarTappedKeys.valueOf(it) }
        )
        is Event.EnteredUrl -> EventWrapper(
            { Events.enteredUrl.record(it) },
            { Events.enteredUrlKeys.valueOf(it) }
        )
        is Event.PerformedSearch -> EventWrapper(
            {
                Metrics.searchCount[this.eventSource.countLabel].add(1)
                Events.performedSearch.record(it)
            },
            { Events.performedSearchKeys.valueOf(it) }
        )
        is Event.SearchWithAds -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.withAds[label].add(1)
            }
        )
        is Event.SearchAdClicked -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.adClicks[label].add(1)
            }
        )
        is Event.SearchInContent -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.inContent[label].add(1)
            }
        )
        is Event.SearchShortcutSelected -> EventWrapper(
            { SearchShortcuts.selected.record(it) },
            { SearchShortcuts.selectedKeys.valueOf(it) }
        )
        is Event.LoginDialogPromptDisplayed -> EventWrapper<NoExtraKeys>(
            { LoginDialog.displayed.record(it) }
        )
        is Event.LoginDialogPromptCancelled -> EventWrapper<NoExtraKeys>(
            { LoginDialog.cancelled.record(it) }
        )
        is Event.LoginDialogPromptSave -> EventWrapper<NoExtraKeys>(
            { LoginDialog.saved.record(it) }
        )
        is Event.LoginDialogPromptNeverSave -> EventWrapper<NoExtraKeys>(
            { LoginDialog.neverSave.record(it) }
        )
        is Event.FindInPageOpened -> EventWrapper<NoExtraKeys>(
            { FindInPage.opened.record(it) }
        )
        is Event.FindInPageClosed -> EventWrapper<NoExtraKeys>(
            { FindInPage.closed.record(it) }
        )
        is Event.FindInPageSearchCommitted -> EventWrapper<NoExtraKeys>(
            { FindInPage.searchedPage.record(it) }
        )
        is Event.ContextMenuItemTapped -> EventWrapper(
            { ContextMenu.itemTapped.record(it) },
            { ContextMenu.itemTappedKeys.valueOf(it) }
        )
        is Event.CrashReporterOpened -> EventWrapper<NoExtraKeys>(
            { CrashReporter.opened.record(it) }
        )
        is Event.CrashReporterClosed -> EventWrapper(
            { CrashReporter.closed.record(it) },
            { CrashReporter.closedKeys.valueOf(it) }
        )
        is Event.BrowserMenuItemTapped -> EventWrapper(
            { Events.browserMenuAction.record(it) },
            { Events.browserMenuActionKeys.valueOf(it) }
        )
        is Event.SetDefaultBrowserToolbarMenuClicked -> EventWrapper<NoExtraKeys>(
            { ExperimentsDefaultBrowser.toolbarMenuClicked.record(it) }
        )
        is Event.ToolbarMenuShown -> EventWrapper<NoExtraKeys>(
            { Events.toolbarMenuVisible.record(it) }
        )
        is Event.ChangedToDefaultBrowser -> EventWrapper<NoExtraKeys>(
            { Events.defaultBrowserChanged.record(it) }
        )
        is Event.DefaultBrowserNotifTapped -> EventWrapper<NoExtraKeys>(
            { Events.defaultBrowserNotifTapped.record(it) }
        )
        is Event.OpenedBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.open.record(it) }
        )
        is Event.OpenedBookmarkInNewTab -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.openInNewTab.record(it) }
        )
        is Event.OpenedBookmarksInNewTabs -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.openInNewTabs.record(it) }
        )
        is Event.OpenedBookmarkInPrivateTab -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.openInPrivateTab.record(it) }
        )
        is Event.OpenedBookmarksInPrivateTabs -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.openInPrivateTabs.record(it) }
        )
        is Event.EditedBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.edited.record(it) }
        )
        is Event.MovedBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.moved.record(it) }
        )
        is Event.RemoveBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.removed.record(it) }
        )
        is Event.RemoveBookmarks -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.multiRemoved.record(it) }
        )
        is Event.ShareBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.shared.record(it) }
        )
        is Event.CopyBookmark -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.copied.record(it) }
        )
        is Event.AddBookmarkFolder -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.folderAdd.record(it) }
        )
        is Event.RemoveBookmarkFolder -> EventWrapper<NoExtraKeys>(
            { BookmarksManagement.folderRemove.record(it) }
        )
        is Event.CustomTabsMenuOpened -> EventWrapper<NoExtraKeys>(
            { CustomTab.menu.record(it) }
        )
        is Event.CustomTabsActionTapped -> EventWrapper<NoExtraKeys>(
            { CustomTab.actionButton.record(it) }
        )
        is Event.CustomTabsClosed -> EventWrapper<NoExtraKeys>(
            { CustomTab.closed.record(it) }
        )
        is Event.UriOpened -> EventWrapper<NoExtraKeys>(
            { Events.totalUriCount.add(1) }
        )
        is Event.NormalAndPrivateUriOpened -> EventWrapper<NoExtraKeys>(
            { Events.normalAndPrivateUriCount.add(1) }
        )
        is Event.ErrorPageVisited -> EventWrapper(
            { ErrorPage.visitedError.record(it) },
            { ErrorPage.visitedErrorKeys.valueOf(it) }
        )
        is Event.SyncAuthOpened -> EventWrapper<NoExtraKeys>(
            { SyncAuth.opened.record(it) }
        )
        is Event.SyncAuthClosed -> EventWrapper<NoExtraKeys>(
            { SyncAuth.closed.record(it) }
        )
        is Event.SyncAuthUseEmail -> EventWrapper<NoExtraKeys>(
            { SyncAuth.useEmail.record(it) }
        )
        is Event.SyncAuthUseEmailProblem -> EventWrapper<NoExtraKeys>(
            { SyncAuth.useEmailProblem.record(it) }
        )
        is Event.SyncAuthSignIn -> EventWrapper<NoExtraKeys>(
            { SyncAuth.signIn.record(it) }
        )
        is Event.SyncAuthSignUp -> EventWrapper<NoExtraKeys>(
            { SyncAuth.signUp.record(it) }
        )
        is Event.SyncAuthPaired -> EventWrapper<NoExtraKeys>(
            { SyncAuth.paired.record(it) }
        )
        is Event.SyncAuthOtherExternal -> EventWrapper<NoExtraKeys>(
            { SyncAuth.otherExternal.record(it) }
        )
        is Event.SyncAuthRecovered -> EventWrapper<NoExtraKeys>(
            { SyncAuth.recovered.record(it) }
        )
        is Event.SyncAuthSignOut -> EventWrapper<NoExtraKeys>(
            { SyncAuth.signOut.record(it) }
        )
        is Event.SyncAuthScanPairing -> EventWrapper<NoExtraKeys>(
            { SyncAuth.scanPairing.record(it) }
        )
        is Event.SyncAccountOpened -> EventWrapper<NoExtraKeys>(
            { SyncAccount.opened.record(it) }
        )
        is Event.SyncAccountSyncNow -> EventWrapper<NoExtraKeys>(
            { SyncAccount.syncNow.record(it) }
        )
        is Event.SignInToSendTab -> EventWrapper<NoExtraKeys>(
            { SyncAccount.signInToSendTab.record(it) }
        )
        is Event.SendTab -> EventWrapper<NoExtraKeys>(
            { SyncAccount.sendTab.record(it) }
        )
        is Event.PreferenceToggled -> EventWrapper(
            { Events.preferenceToggled.record(it) },
            { Events.preferenceToggledKeys.valueOf(it) }
        )
        is Event.HistoryOpened -> EventWrapper<NoExtraKeys>(
            { History.opened.record(it) }
        )
        is Event.HistoryItemShared -> EventWrapper<NoExtraKeys>(
            { History.shared.record(it) }
        )
        is Event.HistoryItemOpened -> EventWrapper<NoExtraKeys>(
            { History.openedItem.record(it) }
        )
        is Event.HistoryOpenedInNewTab -> EventWrapper<NoExtraKeys>(
            { History.openedItemInNewTab.record(it) }
        )
        is Event.HistoryOpenedInNewTabs -> EventWrapper<NoExtraKeys>(
            { History.openedItemsInNewTabs.record(it) }
        )
        is Event.HistoryOpenedInPrivateTab -> EventWrapper<NoExtraKeys>(
            { History.openedItemInPrivateTab.record(it) }
        )
        is Event.HistoryOpenedInPrivateTabs -> EventWrapper<NoExtraKeys>(
            { History.openedItemsInPrivateTabs.record(it) }
        )
        is Event.HistoryItemRemoved -> EventWrapper<NoExtraKeys>(
            { History.removed.record(it) }
        )
        is Event.HistoryAllItemsRemoved -> EventWrapper<NoExtraKeys>(
            { History.removedAll.record(it) }
        )
        is Event.CollectionRenamed -> EventWrapper<NoExtraKeys>(
            { Collections.renamed.record(it) }
        )
        is Event.CollectionTabRestored -> EventWrapper<NoExtraKeys>(
            { Collections.tabRestored.record(it) }
        )
        is Event.CollectionAllTabsRestored -> EventWrapper<NoExtraKeys>(
            { Collections.allTabsRestored.record(it) }
        )
        is Event.CollectionTabRemoved -> EventWrapper<NoExtraKeys>(
            { Collections.tabRemoved.record(it) }
        )
        is Event.CollectionShared -> EventWrapper<NoExtraKeys>(
            { Collections.shared.record(it) }
        )
        is Event.CollectionRemoved -> EventWrapper<NoExtraKeys>(
            { Collections.removed.record(it) }
        )
        is Event.CollectionTabSelectOpened -> EventWrapper<NoExtraKeys>(
            { Collections.tabSelectOpened.record(it) }
        )
        is Event.ReaderModeAvailable -> EventWrapper<NoExtraKeys>(
            { ReaderMode.available.record(it) }
        )
        is Event.ReaderModeOpened -> EventWrapper<NoExtraKeys>(
            { ReaderMode.opened.record(it) }
        )
        is Event.ReaderModeClosed -> EventWrapper<NoExtraKeys>(
            { ReaderMode.closed.record(it) }
        )
        is Event.ReaderModeAppearanceOpened -> EventWrapper<NoExtraKeys>(
            { ReaderMode.appearance.record(it) }
        )
        is Event.CollectionTabLongPressed -> EventWrapper<NoExtraKeys>(
            { Collections.longPress.record(it) }
        )
        is Event.CollectionSaveButtonPressed -> EventWrapper(
            { Collections.saveButton.record(it) },
            { Collections.saveButtonKeys.valueOf(it) }
        )
        is Event.CollectionAddTabPressed -> EventWrapper<NoExtraKeys>(
            { Collections.addTabButton.record(it) }
        )
        is Event.CollectionRenamePressed -> EventWrapper<NoExtraKeys>(
            { Collections.renameButton.record(it) }
        )
        is Event.CollectionSaved -> EventWrapper(
            { Collections.saved.record(it) },
            { Collections.savedKeys.valueOf(it) }
        )
        is Event.CollectionTabsAdded -> EventWrapper(
            { Collections.tabsAdded.record(it) },
            { Collections.tabsAddedKeys.valueOf(it) }
        )
        is Event.SearchWidgetNewTabPressed -> EventWrapper<NoExtraKeys>(
            { SearchWidget.newTabButton.record(it) }
        )
        is Event.SearchWidgetVoiceSearchPressed -> EventWrapper<NoExtraKeys>(
            { SearchWidget.voiceButton.record(it) }
        )
        is Event.PrivateBrowsingSnackbarUndoTapped -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingMode.snackbarUndo.record(it) }
        )
        is Event.PrivateBrowsingNotificationTapped -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingMode.notificationTapped.record(it) }
        )
        is Event.PrivateBrowsingCreateShortcut -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.createShortcut.record(it) }
        )
        is Event.PrivateBrowsingAddShortcutCFR -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.cfrAddShortcut.record(it) }
        )
        is Event.PrivateBrowsingCancelCFR -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.cfrCancel.record(it) }
        )
        is Event.PrivateBrowsingPinnedShortcutPrivateTab -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.pinnedShortcutPriv.record(it) }
        )
        is Event.PrivateBrowsingStaticShortcutTab -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.staticShortcutTab.record(it) }
        )
        is Event.PrivateBrowsingStaticShortcutPrivateTab -> EventWrapper<NoExtraKeys>(
            { PrivateBrowsingShortcut.staticShortcutPriv.record(it) }
        )
        is Event.WhatsNewTapped -> EventWrapper<NoExtraKeys>(
            { Events.whatsNewTapped.record(it) }
        )
        is Event.TabMediaPlay -> EventWrapper<NoExtraKeys>(
            { Tab.mediaPlay.record(it) }
        )
        is Event.TabMediaPause -> EventWrapper<NoExtraKeys>(
            { Tab.mediaPause.record(it) }
        )
        is Event.MediaPlayState -> EventWrapper<NoExtraKeys>(
            { MediaState.play.record(it) }
        )
        is Event.MediaPauseState -> EventWrapper<NoExtraKeys>(
            { MediaState.pause.record(it) }
        )
        is Event.MediaStopState -> EventWrapper<NoExtraKeys>(
            { MediaState.stop.record(it) }
        )
        is Event.MediaFullscreenState -> EventWrapper<NoExtraKeys>(
            { MediaState.fullscreen.record(it) }
        )
        is Event.MediaPictureInPictureState -> EventWrapper<NoExtraKeys>(
            { MediaState.pictureInPicture.record(it) }
        )
        is Event.InAppNotificationDownloadOpen -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.inAppOpen.record(it) }
        )
        is Event.InAppNotificationDownloadTryAgain -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.inAppTryAgain.record(it) }
        )
        is Event.NotificationDownloadCancel -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.cancel.record(it) }
        )
        is Event.NotificationDownloadOpen -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.open.record(it) }
        )
        is Event.NotificationDownloadPause -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.pause.record(it) }
        )
        is Event.NotificationDownloadResume -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.resume.record(it) }
        )
        is Event.NotificationDownloadTryAgain -> EventWrapper<NoExtraKeys>(
            { DownloadNotification.tryAgain.record(it) }
        )
        is Event.DownloadAdded -> EventWrapper<NoExtraKeys>(
            { DownloadsMisc.downloadAdded.record(it) }
        )
        is Event.DownloadsScreenOpened -> EventWrapper<NoExtraKeys>(
            { DownloadsManagement.downloadsScreenOpened.record(it) }
        )
        is Event.DownloadsItemOpened -> EventWrapper<NoExtraKeys>(
            { DownloadsManagement.itemOpened.record(it) }
        )
        is Event.DownloadsItemDeleted -> EventWrapper<NoExtraKeys>(
            { DownloadsManagement.itemDeleted.record(it) }
        )
        is Event.NotificationMediaPlay -> EventWrapper<NoExtraKeys>(
            { MediaNotification.play.record(it) }
        )
        is Event.NotificationMediaPause -> EventWrapper<NoExtraKeys>(
            { MediaNotification.pause.record(it) }
        )
        is Event.TrackingProtectionTrackerList -> EventWrapper<NoExtraKeys>(
            { TrackingProtection.etpTrackerList.record(it) }
        )
        is Event.TrackingProtectionIconPressed -> EventWrapper<NoExtraKeys>(
            { TrackingProtection.etpShield.record(it) }
        )
        is Event.TrackingProtectionSettingsPanel -> EventWrapper<NoExtraKeys>(
            { TrackingProtection.panelSettings.record(it) }
        )
        is Event.TrackingProtectionSettings -> EventWrapper<NoExtraKeys>(
            { TrackingProtection.etpSettings.record(it) }
        )
        is Event.TrackingProtectionException -> EventWrapper<NoExtraKeys>(
            { TrackingProtection.exceptionAdded.record(it) }
        )
        is Event.TrackingProtectionSettingChanged -> EventWrapper(
            { TrackingProtection.etpSettingChanged.record(it) },
            { TrackingProtection.etpSettingChangedKeys.valueOf(it) }
        )
        is Event.OpenedLink -> EventWrapper(
            { Events.openedLink.record(it) },
            { Events.openedLinkKeys.valueOf(it) }
        )
        is Event.OpenLogins -> EventWrapper<NoExtraKeys>(
            { Logins.openLogins.record(it) }
        )
        is Event.OpenOneLogin -> EventWrapper<NoExtraKeys>(
            { Logins.openIndividualLogin.record(it) }
        )
        is Event.CopyLogin -> EventWrapper<NoExtraKeys>(
            { Logins.copyLogin.record(it) }
        )
        is Event.ViewLoginPassword -> EventWrapper<NoExtraKeys>(
            { Logins.viewPasswordLogin.record(it) }
        )
        is Event.DeleteLogin -> EventWrapper<NoExtraKeys>(
            { Logins.deleteSavedLogin.record(it) }
        )
        is Event.EditLogin -> EventWrapper<NoExtraKeys>(
            { Logins.openLoginEditor.record(it) }
        )
        is Event.EditLoginSave -> EventWrapper<NoExtraKeys>(
            { Logins.saveEditedLogin.record(it) }
        )
        is Event.PrivateBrowsingShowSearchSuggestions -> EventWrapper<NoExtraKeys>(
            { SearchSuggestions.enableInPrivate.record(it) }
        )
        is Event.ToolbarPositionChanged -> EventWrapper(
            { ToolbarSettings.changedPosition.record(it) },
            { ToolbarSettings.changedPositionKeys.valueOf(it) }
        )
        is Event.CustomEngineAdded -> EventWrapper<NoExtraKeys>(
            { UserSpecifiedSearchEngines.customEngineAdded.record(it) }
        )
        is Event.CustomEngineDeleted -> EventWrapper<NoExtraKeys>(
            { UserSpecifiedSearchEngines.customEngineDeleted.record(it) }
        )
        is Event.SaveLoginsSettingChanged -> EventWrapper(
            { Logins.saveLoginsSettingChanged.record(it) },
            { Logins.saveLoginsSettingChangedKeys.valueOf(it) }
        )
        is Event.TopSiteOpenDefault -> EventWrapper<NoExtraKeys>(
            { TopSites.openDefault.record(it) }
        )
        is Event.TopSiteOpenGoogle -> EventWrapper<NoExtraKeys>(
            { TopSites.openGoogleSearchAttribution.record(it) }
        )
        is Event.TopSiteOpenBaidu -> EventWrapper<NoExtraKeys>(
            { TopSites.openBaiduSearchAttribution.record(it) }
        )
        is Event.TopSiteOpenFrecent -> EventWrapper<NoExtraKeys>(
            { TopSites.openFrecency.record(it) }
        )
        is Event.TopSiteOpenPinned -> EventWrapper<NoExtraKeys>(
            { TopSites.openPinned.record(it) }
        )
        is Event.TopSiteOpenInNewTab -> EventWrapper<NoExtraKeys>(
            { TopSites.openInNewTab.record(it) }
        )
        is Event.TopSiteOpenInPrivateTab -> EventWrapper<NoExtraKeys>(
            { TopSites.openInPrivateTab.record(it) }
        )
        is Event.TopSiteRemoved -> EventWrapper<NoExtraKeys>(
            { TopSites.remove.record(it) }
        )
        is Event.TopSiteLongPress -> EventWrapper(
            { TopSites.longPress.record(it) },
            { TopSites.longPressKeys.valueOf(it) }
        )
        is Event.TopSiteSwipeCarousel -> EventWrapper(
            { TopSites.swipeCarousel.record(it) },
            { TopSites.swipeCarouselKeys.valueOf(it) }
        )
        is Event.SupportTapped -> EventWrapper<NoExtraKeys>(
            { AboutPage.supportTapped.record(it) }
        )
        is Event.PrivacyNoticeTapped -> EventWrapper<NoExtraKeys>(
            { AboutPage.privacyNoticeTapped.record(it) }
        )
        is Event.PocketTopSiteClicked -> EventWrapper<NoExtraKeys>(
            { Pocket.pocketTopSiteClicked.record(it) }
        )
        is Event.PocketTopSiteRemoved -> EventWrapper<NoExtraKeys>(
            { Pocket.pocketTopSiteRemoved.record(it) }
        )
        is Event.DarkThemeSelected -> EventWrapper(
            { AppTheme.darkThemeSelected.record(it) },
            { AppTheme.darkThemeSelectedKeys.valueOf(it) }
        )
        is Event.AddonsOpenInSettings -> EventWrapper<NoExtraKeys>(
            { Addons.openAddonsInSettings.record(it) }
        )
        is Event.AddonsOpenInToolbarMenu -> EventWrapper(
            { Addons.openAddonInToolbarMenu.record(it) },
            { Addons.openAddonInToolbarMenuKeys.valueOf(it) }
        )
        is Event.AddonOpenSetting -> EventWrapper(
            { Addons.openAddonSetting.record(it) },
            { Addons.openAddonSettingKeys.valueOf(it) }
        )
        is Event.TipDisplayed -> EventWrapper(
            { Tip.displayed.record(it) },
            { Tip.displayedKeys.valueOf(it) }
        )
        is Event.TipPressed -> EventWrapper(
            { Tip.pressed.record(it) },
            { Tip.pressedKeys.valueOf(it) }
        )
        is Event.TipClosed -> EventWrapper(
            { Tip.closed.record(it) },
            { Tip.closedKeys.valueOf(it) }
        )
        is Event.VoiceSearchTapped -> EventWrapper<NoExtraKeys>(
            { VoiceSearch.tapped.record(it) }
        )
        is Event.TabCounterMenuItemTapped -> EventWrapper(
            { Events.tabCounterMenuAction.record(it) },
            { Events.tabCounterMenuActionKeys.valueOf(it) }
        )
        is Event.OnboardingPrivateBrowsing -> EventWrapper<NoExtraKeys>(
            { Onboarding.prefToggledPrivateBrowsing.record(it) }
        )
        is Event.OnboardingPrivacyNotice -> EventWrapper<NoExtraKeys>(
            { Onboarding.privacyNotice.record(it) }
        )
        is Event.OnboardingManualSignIn -> EventWrapper<NoExtraKeys>(
            { Onboarding.fxaManualSignin.record(it) }
        )
        is Event.OnboardingAutoSignIn -> EventWrapper<NoExtraKeys>(
            { Onboarding.fxaAutoSignin.record(it) }
        )
        is Event.OnboardingFinish -> EventWrapper<NoExtraKeys>(
            { Onboarding.finish.record(it) }
        )
        is Event.OnboardingTrackingProtection -> EventWrapper(
            { Onboarding.prefToggledTrackingProt.record(it) },
            { Onboarding.prefToggledTrackingProtKeys.valueOf(it) }
        )
        is Event.OnboardingThemePicker -> EventWrapper(
            { Onboarding.prefToggledThemePicker.record(it) },
            { Onboarding.prefToggledThemePickerKeys.valueOf(it) }
        )
        is Event.OnboardingToolbarPosition -> EventWrapper(
            { Onboarding.prefToggledToolbarPosition.record(it) },
            { Onboarding.prefToggledToolbarPositionKeys.valueOf(it) }
        )

        is Event.TabsTrayOpened -> EventWrapper<NoExtraKeys>(
            { TabsTray.opened.record(it) }
        )
        is Event.TabsTrayClosed -> EventWrapper<NoExtraKeys>(
            { TabsTray.closed.record(it) }
        )
        is Event.OpenedExistingTab -> EventWrapper<NoExtraKeys>(
            { TabsTray.openedExistingTab.record(it) }
        )
        is Event.ClosedExistingTab -> EventWrapper<NoExtraKeys>(
            { TabsTray.closedExistingTab.record(it) }
        )
        is Event.TabsTrayPrivateModeTapped -> EventWrapper<NoExtraKeys>(
            { TabsTray.privateModeTapped.record(it) }
        )
        is Event.TabsTrayNormalModeTapped -> EventWrapper<NoExtraKeys>(
            { TabsTray.normalModeTapped.record(it) }
        )
        is Event.TabsTraySyncedModeTapped -> EventWrapper<NoExtraKeys>(
            { TabsTray.syncedModeTapped.record(it) }
        )
        is Event.NewTabTapped -> EventWrapper<NoExtraKeys>(
            { TabsTray.newTabTapped.record(it) }
        )
        is Event.NewPrivateTabTapped -> EventWrapper<NoExtraKeys>(
            { TabsTray.newPrivateTabTapped.record(it) }
        )
        is Event.TabsTrayMenuOpened -> EventWrapper<NoExtraKeys>(
            { TabsTray.menuOpened.record(it) }
        )
        is Event.TabsTraySaveToCollectionPressed -> EventWrapper<NoExtraKeys>(
            { TabsTray.saveToCollection.record(it) }
        )
        is Event.TabsTrayShareAllTabsPressed -> EventWrapper<NoExtraKeys>(
            { TabsTray.shareAllTabs.record(it) }
        )
        is Event.TabsTrayCloseAllTabsPressed -> EventWrapper<NoExtraKeys>(
            { TabsTray.closeAllTabs.record(it) }
        )
        is Event.TabsTrayRecentlyClosedPressed -> EventWrapper<NoExtraKeys>(
            { TabsTray.inactiveTabsRecentlyClosed.record(it) }
        )
        is Event.AutoPlaySettingVisited -> EventWrapper<NoExtraKeys>(
            { Autoplay.visitedSetting.record(it) }
        )
        is Event.AutoPlaySettingChanged -> EventWrapper(
            { Autoplay.settingChanged.record(it) },
            { Autoplay.settingChangedKeys.valueOf(it) }
        )
        is Event.ProgressiveWebAppOpenFromHomescreenTap -> EventWrapper<NoExtraKeys>(
            { ProgressiveWebApp.homescreenTap.record(it) }
        )
        is Event.ProgressiveWebAppInstallAsShortcut -> EventWrapper<NoExtraKeys>(
            { ProgressiveWebApp.installTap.record(it) }
        )
        is Event.ProgressiveWebAppForeground -> EventWrapper(
            { ProgressiveWebApp.foreground.record(it) },
            { ProgressiveWebApp.foregroundKeys.valueOf(it) }
        )
        is Event.ProgressiveWebAppBackground -> EventWrapper(
            { ProgressiveWebApp.background.record(it) },
            { ProgressiveWebApp.backgroundKeys.valueOf(it) }
        )
        is Event.CopyUrlUsed -> EventWrapper<NoExtraKeys>(
            { Events.copyUrlTapped.record(it) }
        )

        is Event.SyncedTabOpened -> EventWrapper<NoExtraKeys>(
            { Events.syncedTabOpened.record(it) }
        )

        is Event.RecentlyClosedTabsOpened -> EventWrapper<NoExtraKeys>(
            { Events.recentlyClosedTabsOpened.record(it) }
        )

        is Event.MasterPasswordMigrationDisplayed -> EventWrapper<NoExtraKeys>(
            { MasterPassword.displayed.record(it) }
        )
        is Event.MasterPasswordMigrationSuccess -> EventWrapper<NoExtraKeys>(
            { MasterPassword.migration.record(it) }
        )
        is Event.TabSettingsOpened -> EventWrapper<NoExtraKeys>(
            { Tabs.settingOpened.record(it) }
        )
        Event.ContextMenuCopyTapped -> EventWrapper<NoExtraKeys>(
            { ContextualMenu.copyTapped.record(it) }
        )
        is Event.ContextMenuSearchTapped -> EventWrapper<NoExtraKeys>(
            { ContextualMenu.searchTapped.record(it) }
        )
        is Event.ContextMenuSelectAllTapped -> EventWrapper<NoExtraKeys>(
            { ContextualMenu.selectAllTapped.record(it) }
        )
        is Event.ContextMenuShareTapped -> EventWrapper<NoExtraKeys>(
            { ContextualMenu.shareTapped.record(it) }
        )
        Event.HaveOpenTabs -> EventWrapper<NoExtraKeys>(
            { Metrics.hasOpenTabs.set(true) }
        )
        Event.HaveNoOpenTabs -> EventWrapper<NoExtraKeys>(
            { Metrics.hasOpenTabs.set(false) }
        )
        is Event.BannerOpenInAppDisplayed -> EventWrapper<NoExtraKeys>(
            { BannerOpenInApp.displayed.record(it) }
        )
        is Event.BannerOpenInAppDismissed -> EventWrapper<NoExtraKeys>(
            { BannerOpenInApp.dismissed.record(it) }
        )
        is Event.BannerOpenInAppGoToSettings -> EventWrapper<NoExtraKeys>(
            { BannerOpenInApp.goToSettings.record(it) }
        )
        is Event.SyncedTabSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { SyncedTabs.syncedTabsSuggestionClicked.record(it) }
        )

        is Event.BookmarkSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.bookmarkSuggestionClicked.record(it) }
        )
        is Event.ClipboardSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.clipboardSuggestionClicked.record(it) }
        )
        is Event.HistorySuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.historySuggestionClicked.record(it) }
        )
        is Event.SearchActionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.searchActionClicked.record(it) }
        )
        is Event.SearchSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.searchSuggestionClicked.record(it) }
        )
        is Event.OpenedTabSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.openedTabSuggestionClicked.record(it) }
        )

        is Event.SecurePrefsExperimentFailure -> EventWrapper(
            { AndroidKeystoreExperiment.experimentFailure.record(it) },
            { AndroidKeystoreExperiment.experimentFailureKeys.valueOf(it) }
        )
        is Event.SecurePrefsGetFailure -> EventWrapper(
            { AndroidKeystoreExperiment.getFailure.record(it) },
            { AndroidKeystoreExperiment.getFailureKeys.valueOf(it) }
        )
        is Event.SecurePrefsGetSuccess -> EventWrapper(
            { AndroidKeystoreExperiment.getResult.record(it) },
            { AndroidKeystoreExperiment.getResultKeys.valueOf(it) }
        )
        is Event.SecurePrefsWriteFailure -> EventWrapper(
            { AndroidKeystoreExperiment.writeFailure.record(it) },
            { AndroidKeystoreExperiment.writeFailureKeys.valueOf(it) }
        )
        is Event.SecurePrefsWriteSuccess -> EventWrapper<NoExtraKeys>(
            { AndroidKeystoreExperiment.writeSuccess.record(it) }
        )
        is Event.SecurePrefsReset -> EventWrapper<NoExtraKeys>(
            { AndroidKeystoreExperiment.reset.record(it) }
        )
        is Event.HomeMenuSettingsItemClicked -> EventWrapper<NoExtraKeys>(
            { HomeMenu.settingsItemClicked.record(it) }
        )

        is Event.CloseExperimentCardClicked -> EventWrapper<NoExtraKeys>(
            { SetDefaultNewtabExperiment.closeExperimentCardClicked.record(it) }
        )
        is Event.SetDefaultBrowserNewTabClicked -> EventWrapper<NoExtraKeys>(
            { SetDefaultNewtabExperiment.setDefaultBrowserClicked.record(it) }
        )
        is Event.SetDefaultBrowserSettingsScreenClicked -> EventWrapper<NoExtraKeys>(
            { SetDefaultSettingExperiment.setDefaultBrowserClicked.record(it) }
        )
        is Event.HomeScreenDisplayed -> EventWrapper<NoExtraKeys>(
            { HomeScreen.homeScreenDisplayed.record(it) }
        )
        is Event.TabViewSettingChanged -> EventWrapper(
            { Events.tabViewChanged.record(it) },
            { Events.tabViewChangedKeys.valueOf(it) }
        )

        is Event.BrowserToolbarHomeButtonClicked -> EventWrapper<NoExtraKeys>(
            { Events.browserToolbarHomeTapped.record(it) }
        )

        is Event.StartOnHomeEnterHomeScreen -> EventWrapper<NoExtraKeys>(
            { StartOnHome.enterHomeScreen.record(it) }
        )

        is Event.StartOnHomeOpenTabsTray -> EventWrapper<NoExtraKeys>(
            { StartOnHome.openTabsTray.record(it) }
        )

        is Event.OpenRecentTab -> EventWrapper<NoExtraKeys>(
            { RecentTabs.recentTabOpened.record(it) }
        )

        is Event.OpenInProgressMediaTab -> EventWrapper<NoExtraKeys>(
            { RecentTabs.inProgressMediaTabOpened.record(it) }
        )

        is Event.ShowAllRecentTabs -> EventWrapper<NoExtraKeys>(
            { RecentTabs.showAllClicked.record(it) }
        )

        is Event.AndroidAutofillRequestWithLogins -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.requestMatchingLogins.record(it) }
        )
        is Event.AndroidAutofillRequestWithoutLogins -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.requestNoMatchingLogins.record(it) }
        )
        is Event.AndroidAutofillSearchDisplayed -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.searchDisplayed.record(it) }
        )
        is Event.AndroidAutofillSearchItemSelected -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.searchItemSelected.record(it) }
        )
        is Event.AndroidAutofillUnlockCanceled -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.unlockCancelled.record(it) }
        )
        is Event.AndroidAutofillUnlockSuccessful -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.unlockSuccessful.record(it) }
        )
        is Event.AndroidAutofillConfirmationCanceled -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.confirmCancelled.record(it) }
        )
        is Event.AndroidAutofillConfirmationSuccessful -> EventWrapper<NoExtraKeys>(
            { AndroidAutofill.confirmSuccessful.record(it) }
        )

        // Don't record other events in Glean:
        is Event.AddBookmark -> null
        is Event.OpenedAppFirstRun -> null
        is Event.InteractWithSearchURLArea -> null
        is Event.ClearedPrivateData -> null
        is Event.DismissedOnboarding -> null
        is Event.FennecToFenixMigrated -> null
        is Event.AddonInstalled -> null
        is Event.SearchWidgetInstalled -> null
        is Event.SyncAuthFromSharedReuse, Event.SyncAuthFromSharedCopy -> null
    }

/**
 * Service responsible for sending the activation and installation pings.
 */
class GleanMetricsService(
    private val context: Context
) : MetricsService {
    override val type = MetricServiceType.Data

    private val logger = Logger("GleanMetricsService")
    private var initialized = false

    private val activationPing = ActivationPing(context)
    private val installationPing = FirstSessionPing(context)

    override fun start() {
        logger.debug("Enabling Glean.")
        // Initialization of Glean already happened in FenixApplication.
        Glean.setUploadEnabled(true)

        if (initialized) return
        initialized = true

        // The code below doesn't need to execute immediately, so we'll add them to the visual
        // completeness task queue to be run later.
        context.components.performance.visualCompletenessQueue.queue.runIfReadyOrQueue {
            // We have to initialize Glean *on* the main thread, because it registers lifecycle
            // observers. However, the activation ping must be sent *off* of the main thread,
            // because it calls Google ad APIs that must be called *off* of the main thread.
            // These two things actually happen in parallel, but that should be ok because Glean
            // can handle events being recorded before it's initialized.
            Glean.registerPings(Pings)

            activationPing.checkAndSend()
            installationPing.checkAndSend()
        }
    }

    override fun stop() {
        Glean.setUploadEnabled(false)
    }

    override fun track(event: Event) {
        event.wrapper?.track(event)
    }

    override fun shouldTrack(event: Event): Boolean {
        return event.wrapper != null
    }
}

// Helper function for making our booleans fit into the string list formatting
fun Boolean.toStringList(): List<String> {
    return listOf(this.toString())
}
