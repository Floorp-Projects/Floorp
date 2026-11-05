/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { ShortcutUtils } from "resource://gre/modules/ShortcutUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetters(lazy, {
  WindowsUIUtils: ["@mozilla.org/windows-ui-utils;1", Ci.nsIWindowsUIUtils],
});

ChromeUtils.defineESModuleGetters(lazy, {
  ASRouter:
    // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
    "resource:///modules/asrouter/ASRouter.sys.mjs",
  PageActions: "resource:///modules/PageActions.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

import { Rect, Point } from "resource://gre/modules/Geometry.sys.mjs";

const PLAYER_URI = "chrome://global/content/pictureinpicture/player.xhtml";
// Currently, we need titlebar="yes" on macOS in order for the player window
// to be resizable. See bug 1824171.
const TITLEBAR = AppConstants.platform == "macosx" ? "yes" : "no";
const PLAYER_FEATURES = `chrome,alwaysontop,lockaspectratio,resizable,dialog,mediapip,titlebar=${TITLEBAR}`;

const WINDOW_TYPE = "Toolkit:PictureInPicture";
const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const TOGGLE_FIRST_SEEN_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.first-seen-secs";
const TOGGLE_HAS_USED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.has-used";
const TOGGLE_POSITION_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.position";
const TOGGLE_POSITION_RIGHT = "right";
const TOGGLE_POSITION_LEFT = "left";
const RESIZE_MARGIN_PX = 16;
// Close reasons that should suppress auto-toggle when the originating tab is backgrounded
const DELIBERATE_CLOSE_REASONS = [
  "CloseButton",
  "ClosePlayerShortcut",
  "Shortcut",
];

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PIP_ENABLED",
  "media.videocontrols.picture-in-picture.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PIP_URLBAR_BUTTON",
  "media.videocontrols.picture-in-picture.urlbar-button.enabled",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "RESPECT_PIP_DISABLED",
  "media.videocontrols.picture-in-picture.respect-disablePictureInPicture",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PIP_WHEN_SWITCHING_TABS",
  "media.videocontrols.picture-in-picture.enable-when-switching-tabs.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PIP_WHEN_OCCLUDED",
  "media.videocontrols.picture-in-picture.enable-when-occluded.enabled",
  false
);

// True if the window is minimized or fully covered.
function isWindowOutOfSight(win) {
  return win.windowState == win.STATE_MINIMIZED || win.isFullyOccluded;
}

/**
 * To differentiate windows in the Telemetry Event Log, each Picture-in-Picture
 * player window is given a unique ID.
 */
let gNextWindowID = 0;

export class PictureInPictureLauncherParent extends JSWindowActorParent {
  async receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "PictureInPicture:Request": {
        let videoData = aMessage.data;
        return PictureInPicture.handlePictureInPictureRequest(
          this.manager,
          videoData
        );
      }
    }
    return undefined;
  }
}

export class PictureInPictureToggleParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    let browsingContext = aMessage.target.browsingContext;
    let browser = browsingContext.top.embedderElement;
    switch (aMessage.name) {
      case "PictureInPicture:OpenToggleContextMenu": {
        let win = browser.documentGlobal;
        PictureInPicture.openToggleContextMenu(win, aMessage.data);
        break;
      }
      case "PictureInPicture:UpdateEligiblePipVideoCount": {
        let { pipCount, pipDisabledCount } = aMessage.data;
        PictureInPicture.updateEligiblePipVideoCount(browsingContext, {
          pipCount,
          pipDisabledCount,
        });
        PictureInPicture.updateUrlbarToggle(browser);
        break;
      }
      case "PictureInPicture:SetFirstSeen": {
        let { dateSeconds } = aMessage.data;
        PictureInPicture.setFirstSeen(dateSeconds);
        break;
      }
      case "PictureInPicture:SetHasUsed": {
        let { hasUsed } = aMessage.data;
        PictureInPicture.setHasUsed(hasUsed);
        break;
      }
      case "PictureInPicture:VideoTabHidden": {
        // Skip unless an auto-open pref is on. browser is null if the tab or
        // window has already closed.
        if (
          !lazy.PIP_ENABLED ||
          (!lazy.PIP_WHEN_SWITCHING_TABS && !lazy.PIP_WHEN_OCCLUDED) ||
          !browser
        ) {
          break;
        }
        // The page is hidden because its tab was switched away from
        // (enable-when-switching-tabs), or because its window was minimized or
        // covered while the tab stayed selected (enable-when-occluded).
        let win = browser.documentGlobal;
        let tabStillSelected = win.gBrowser.selectedBrowser == browser;
        if (tabStillSelected) {
          if (
            !lazy.PIP_WHEN_OCCLUDED ||
            !isWindowOutOfSight(win) ||
            PictureInPicture.weakOccludedBrowserClosedDeliberately.has(browser)
          ) {
            break;
          }
        } else if (!lazy.PIP_WHEN_SWITCHING_TABS) {
          break;
        }

        // Suppress auto-toggle if the user deliberately closed this PiP window
        if (
          PictureInPicture.weakAutoPipBrowserClosedDeliberately.has(browser)
        ) {
          PictureInPicture.weakAutoPipBrowserClosedDeliberately.delete(browser);
          break;
        }
        let actor = browsingContext.currentWindowGlobal.getActor(
          "PictureInPictureLauncher"
        );
        actor.sendAsyncMessage("PictureInPicture:AutoToggle");
        break;
      }
    }
  }
}

/**
 * This module is responsible for creating a Picture in Picture window to host
 * a clone of a video element running in web content.
 */
export class PictureInPictureParent extends JSWindowActorParent {
  async receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "PictureInPicture:Resize": {
        let videoData = aMessage.data;
        PictureInPicture.resizePictureInPictureWindow(videoData, this);
        break;
      }
      case "PictureInPicture:Close": {
        /**
         * Content has requested that its Picture in Picture window go away.
         */
        let reason = aMessage.data.reason;
        return PictureInPicture.closeSinglePipWindow({
          reason,
          actorRef: this,
        });
      }
      case "PictureInPicture:Playing": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.setIsPlayingState(true);
        }
        break;
      }
      case "PictureInPicture:Paused": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.setIsPlayingState(false);
        }
        break;
      }
      case "PictureInPicture:Muting": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.setIsMutedState(true);
        }
        break;
      }
      case "PictureInPicture:Unmuting": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.setIsMutedState(false);
        }
        break;
      }
      case "PictureInPicture:EnableSubtitlesButton": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.enableSubtitlesButton();
        }
        break;
      }
      case "PictureInPicture:DisableSubtitlesButton": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.disableSubtitlesButton();
        }
        break;
      }
      case "PictureInPicture:SetTimestampAndScrubberPosition": {
        let { timestamp, scrubberPosition } = aMessage.data;
        let player = PictureInPicture.getWeakPipPlayer(this);
        // The player window may already be closed by the time this async
        // message arrives, in which case there is nothing to update.
        if (player) {
          player.setTimestamp(timestamp);
          player.setScrubberPosition(scrubberPosition);
        }
        break;
      }
      case "PictureInPicture:VolumeChange": {
        let { volume } = aMessage.data;
        let player = PictureInPicture.getWeakPipPlayer(this);
        // The player window may already be closed by the time this async
        // message arrives, in which case there is nothing to update.
        if (player) {
          player.setVolume(volume);
        }
        break;
      }
      case "PictureInPicture:PlaybackRateChange": {
        let { playbackRate } = aMessage.data;
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.setPlaybackRate(playbackRate);
        }
        break;
      }
    }
    return undefined;
  }
}

/**
 * This module is responsible for creating a Picture in Picture window to host
 * a clone of a video element running in web content.
 */
export var PictureInPicture = {
  // Maps PictureInPictureParent actors to their corresponding PiP player windows
  weakPipToWin: new WeakMap(),

  // Maps PiP player windows to their originating content's browser
  weakWinToBrowser: new WeakMap(),

  // Maps a browser to the number of PiP windows it has
  browserWeakMap: new WeakMap(),

  // Maps an AppWindow to the number of PiP windows it has
  originatingWinWeakMap: new WeakMap(),

  // Maps a WindowGlobal to count of eligible PiP videos
  weakGlobalToEligiblePipCount: new WeakMap(),

  // Tracks the 1 pip window we allow to exist from video.requestPictureInPicture()
  apiPipWindow: null,

  // Tracks the number of open player windows for Telemetry tracking.
  currentPlayerCount: 0,
  maxConcurrentPlayerCount: 0,

  // Maps auto pip browser to PictureInPictureParent actor
  weakAutoPipBrowserToParent: new WeakMap(),

  // Browsers with deliberately closed PiP windows; suppresses auto-toggle.
  weakAutoPipBrowserClosedDeliberately: new WeakSet(),

  // Browsers whose player was closed on purpose while their window was out of
  // sight. No auto-toggle for them until the window is shown again.
  weakOccludedBrowserClosedDeliberately: new WeakSet(),

  /**
   * Returns the player window if one exists and if it hasn't yet been closed.
   *
   * @param {PictureInPictureParent} pipActorRef
   *   Reference to the calling PictureInPictureParent actor
   *
   * @returns {Window} the player window if it exists and is not in the
   * process of being closed. Returns null otherwise.
   */
  getWeakPipPlayer(pipActorRef) {
    let playerWin = this.weakPipToWin.get(pipActorRef);
    if (!playerWin || playerWin.closed) {
      return null;
    }
    return playerWin;
  },

  /**
   * Get the PiP panel for a browser. Create the panel if needed.
   *
   * @param {Browser} browser The current browser
   * @returns panel The panel element
   */
  getPanelForBrowser(browser) {
    let panel = browser.ownerDocument.querySelector("#PictureInPicturePanel");

    if (!panel) {
      let template = browser.ownerDocument.querySelector(
        "#PictureInPicturePanelTemplate"
      );
      let clone = template.content.cloneNode(true);
      template.replaceWith(clone);

      panel = this.getPanelForBrowser(browser);
      this._attachEventListeners(panel);
    }
    return panel;
  },

  _attachEventListeners(panel) {
    panel.addEventListener("popupshown", this);
    panel.addEventListener("popuphidden", this);
    panel
      .querySelector("#respect-pipDisabled-switch")
      .addEventListener("click", this);
  },

  handleEvent(event) {
    switch (event.type) {
      case "TabSwapPictureInPicture": {
        this.onPipSwappedBrowsers(event);
        break;
      }
      case "TabSelect": {
        this.unpipAutoPipBrowser(event);
        this.updatePlayingDurationHistograms();
        break;
      }
      case "sizemodechange":
      case "occlusionstatechange": {
        this.unpipOccludedAutoPipBrowser(event.currentTarget);
        break;
      }
      case "popupshown":
        this.onPipPanelShown(event);
        break;
      case "popuphidden":
        this.onPipPanelHidden(event);
        break;
      case "click":
        this.toggleRespectDisablePip(event);
        break;
    }
  },

  /**
   * Increase the count of PiP windows for a given browser
   *
   * @param browser The browser to increase PiP count in browserWeakMap
   */
  addPiPBrowserToWeakMap(browser) {
    let count = this.browserWeakMap.has(browser)
      ? this.browserWeakMap.get(browser)
      : 0;
    this.browserWeakMap.set(browser, count + 1);

    // If a browser is being added to the browserWeakMap, that means its
    // probably a good time to make sure the playing duration histograms
    // are up-to-date, as it means that we've either opened a new PiP
    // player window, or moved the originating tab to another window.
    this.updatePlayingDurationHistograms();
  },

  /**
   * Increase the count of PiP windows for a given AppWindow.
   *
   * @param {Browser} browser
   *   The content browser that the originating video lives in and from which
   *   we'll read its parent window to increase PiP window count in originatingWinWeakMap.
   */
  addOriginatingWinToWeakMap(browser) {
    // Embedded chrome documents share the containing AppWindow. Only its top
    // chrome browsing context can be forced active, including for captions.
    let parentWin = browser.documentGlobal.browsingContext.topChromeWindow;
    let count = this.originatingWinWeakMap.get(parentWin);
    if (!count || count == 0) {
      this.setOriginatingWindowActive(parentWin.browsingContext, true);
      this.originatingWinWeakMap.set(parentWin, 1);

      let gBrowser = parentWin.gBrowser;
      if (gBrowser) {
        gBrowser.tabContainer.addEventListener("TabSelect", this);
      }
      if (lazy.PIP_WHEN_OCCLUDED) {
        parentWin.addEventListener("sizemodechange", this);
        parentWin.addEventListener("occlusionstatechange", this);
      }
    } else {
      this.originatingWinWeakMap.set(parentWin, count + 1);
    }
  },

  /**
   * Decrease the count of PiP windows for a given browser.
   * If the count becomes 0, we will remove the browser from the WeakMap
   *
   * @param browser The browser to decrease PiP count in browserWeakMap
   */
  removePiPBrowserFromWeakMap(browser) {
    let count = this.browserWeakMap.get(browser);
    if (count <= 1) {
      this.browserWeakMap.delete(browser);
      let tabbrowser = browser.getTabBrowser();
      if (tabbrowser && !tabbrowser.shouldActivateDocShell(browser)) {
        browser.docShellIsActive = false;
      }
    } else {
      this.browserWeakMap.set(browser, count - 1);
    }
  },

  /**
   * Decrease the count of PiP windows for a given AppWindow.
   * If the count becomes 0, we will remove the AppWindow from the WeakMap.
   *
   * @param {Browser} browser
   *   The content browser that the originating video lives in and from which
   *   we'll read its parent window to decrease PiP window count in originatingWinWeakMap.
   */
  removeOriginatingWinFromWeakMap(browser) {
    let parentWin = browser?.documentGlobal?.browsingContext.topChromeWindow;

    if (!parentWin) {
      return;
    }

    let count = this.originatingWinWeakMap.get(parentWin);
    if (!count || count <= 1) {
      this.originatingWinWeakMap.delete(parentWin, 0);
      this.setOriginatingWindowActive(parentWin.browsingContext, false);

      let gBrowser = parentWin.gBrowser;
      if (gBrowser) {
        gBrowser.tabContainer.removeEventListener("TabSelect", this);
      }
      parentWin.removeEventListener("sizemodechange", this);
      parentWin.removeEventListener("occlusionstatechange", this);
    } else {
      this.originatingWinWeakMap.set(parentWin, count - 1);
    }
  },

  /**
   * Because we set the docShellIsActive = true on pip browsers, we never get a
   * visibilitychange when we switch back to the pip browser tab. Therefore we
   * use the TabSelect event to detect when we switch back to a tab that was
   * auto pip'd and close the pip window if so.
   * Note: this is the function that closes auto pip and is the counterpart to
   * VideoTabHidden
   */
  unpipAutoPipBrowser(event) {
    let browser = event.target.linkedBrowser;
    if (this.weakAutoPipBrowserToParent.has(browser)) {
      this.closeSinglePipWindow({
        reason: "Foregrounded",
        actorRef: this.weakAutoPipBrowserToParent.get(browser),
      });
    }
  },

  /**
   * Closes a player that auto-opened because this window was minimized or
   * covered while its video tab stayed selected, once the window can be seen
   * again. unpipAutoPipBrowser does the same when the user switches back to
   * a tab.
   *
   * @param {Window} win
   */
  unpipOccludedAutoPipBrowser(win) {
    if (isWindowOutOfSight(win)) {
      return;
    }
    let browser = win.gBrowser?.selectedBrowser;
    if (browser && this.weakAutoPipBrowserToParent.has(browser)) {
      this.closeSinglePipWindow({
        reason: "Foregrounded",
        actorRef: this.weakAutoPipBrowserToParent.get(browser),
      });
    }
  },

  onPipSwappedBrowsers(event) {
    let otherTab = event.detail;
    if (otherTab) {
      if (
        this.weakAutoPipBrowserClosedDeliberately.has(
          event.target.linkedBrowser
        )
      ) {
        this.weakAutoPipBrowserClosedDeliberately.add(otherTab.linkedBrowser);
        this.weakAutoPipBrowserClosedDeliberately.delete(
          event.target.linkedBrowser
        );
      }
      for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
        if (this.weakWinToBrowser.get(win) === event.target.linkedBrowser) {
          this.weakWinToBrowser.set(win, otherTab.linkedBrowser);
          this.removePiPBrowserFromWeakMap(event.target.linkedBrowser);
          this.removeOriginatingWinFromWeakMap(event.target.linkedBrowser);
          this.addPiPBrowserToWeakMap(otherTab.linkedBrowser);
          this.addOriginatingWinToWeakMap(otherTab.linkedBrowser);
        }
        if (this.weakAutoPipBrowserToParent.has(event.target.linkedBrowser)) {
          // Add the new browser with the existing actor
          this.weakAutoPipBrowserToParent.set(
            otherTab.linkedBrowser,
            this.weakAutoPipBrowserToParent.get(event.target.linkedBrowser)
          );
          // Delete the old browser
          this.weakAutoPipBrowserToParent.delete(event.target.linkedBrowser);
        }
      }
      otherTab.addEventListener("TabSwapPictureInPicture", this);
    }
  },

  updatePlayingDurationHistograms() {
    // A tab switch occurred in a browser window with one more tabs that have
    // PiP player windows associated with them.
    for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
      let browser = this.weakWinToBrowser.get(win);
      let gBrowser = browser.getTabBrowser();
      if (gBrowser?.selectedBrowser == browser) {
        // If there are any background stopwatches running for this window, finish
        // them and switch to foreground.
        if (win._backgroundTabTimerId) {
          Glean.pictureinpicture.backgroundTabPlayingDuration.stopAndAccumulate(
            win._backgroundTabTimerId
          );
          win._backgroundTabTimerId = null;
        }
        if (!win._foregroundTabTimerId) {
          win._foregroundTabTimerId =
            Glean.pictureinpicture.foregroundTabPlayingDuration.start();
        }
      } else {
        // If there are any foreground stopwatches running for this window, finish
        // them and switch to background.
        if (win._foregroundTabTimerId) {
          Glean.pictureinpicture.foregroundTabPlayingDuration.stopAndAccumulate(
            win._foregroundTabTimerId
          );
          win._foregroundTabTimerId = null;
        }

        if (!win._backgroundTabTimerId) {
          win._backgroundTabTimerId =
            Glean.pictureinpicture.backgroundTabPlayingDuration.start();
        }
      }
    }
  },

  /**
   * Called when the browser UI handles the View:PictureInPicture command via
   * the keyboard.
   *
   * @param {Event} event
   */
  onCommand(event) {
    if (!lazy.PIP_ENABLED) {
      return;
    }

    let win = event.target.documentGlobal;
    let bc = Services.focus.focusedContentBrowsingContext;
    if (bc.top == win.gBrowser.selectedBrowser.browsingContext) {
      let actor = bc.currentWindowGlobal.getActor("PictureInPictureLauncher");
      actor.sendAsyncMessage("PictureInPicture:KeyToggle");
    }
  },

  async focusTabAndClosePip(window, pipActor) {
    let browser = this.weakWinToBrowser.get(window);
    if (!browser) {
      return;
    }

    let gBrowser = browser.getTabBrowser();
    let tab = gBrowser.getTabForBrowser(browser);

    // "Back to tab" is a deliberate request to return to the originating tab.
    // On macOS the player is a non-activating window (bug 1688932): a plain
    // window focus marks the originating window active but does not order it in
    // front of another application's fullscreen Space, so it stays hidden.
    // Activating the application brings Firefox forward onto that Space.
    if (AppConstants.platform == "macosx") {
      Cc["@mozilla.org/widget/macdocksupport;1"]
        .getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
    }

    // focus the tab's window
    tab.documentGlobal.focus();

    gBrowser.selectedTab = tab;
    await this.closeSinglePipWindow({ reason: "Unpip", actorRef: pipActor });
  },

  /**
   * Update the respect PiPDisabled pref value when the toggle is clicked.
   *
   * @param {Event} event The event from toggling the respect
   *   PiPDisabled in the PiP panel
   */
  toggleRespectDisablePip(event) {
    let toggle = event.target;
    let respectPipDisabled = !toggle.pressed;

    Services.prefs.setBoolPref(
      "media.videocontrols.picture-in-picture.respect-disablePictureInPicture",
      respectPipDisabled
    );

    Glean.pictureinpicture.disrespectDisableUrlBar.record();
  },

  /**
   * Updates the PiP count and PiPDisabled count of eligible PiP videos for a
   * respective WindowGlobal.
   *
   * @param {BrowsingContext} browsingContext The BrowsingContext with eligible videos
   * @param {object} object
   *    pipCount: The number of eligible videos for the respective WindowGlobal
   *    pipDisabledCount: The number of disablePiP videos for the respective WindowGlobal
   */
  updateEligiblePipVideoCount(browsingContext, object) {
    let windowGlobal = browsingContext.currentWindowGlobal;

    if (windowGlobal) {
      this.weakGlobalToEligiblePipCount.set(windowGlobal, object);
    }
  },

  /**
   * A generator function that yeilds a WindowGlobal, it's respective PiP
   * count, and if any of the videos have PiPDisabled set.
   *
   * @param {Browser} browser The selected browser
   */
  *windowGlobalPipCountGenerator(browser) {
    let contextsToVisit = [browser.browsingContext];
    while (contextsToVisit.length) {
      let currentBC = contextsToVisit.pop();
      let windowGlobal = currentBC.currentWindowGlobal;

      if (!windowGlobal) {
        continue;
      }

      let { pipCount, pipDisabledCount } =
        this.weakGlobalToEligiblePipCount.get(windowGlobal) || {
          pipCount: 0,
          pipDisabledCount: 0,
        };

      contextsToVisit.push(...currentBC.children);

      yield { windowGlobal, pipCount, pipDisabledCount };
    }
  },

  /**
   * Gets the total eligible video count and total PiPDisabled count for a
   * given browser.
   *
   * @param {Browser} browser The selected browser
   * @returns Total count of eligible PiP videos for the selected broser
   */
  getEligiblePipVideoCount(browser) {
    let totalPipCount = 0;
    let totalPipDisabled = 0;

    for (let {
      pipCount,
      pipDisabledCount,
    } of this.windowGlobalPipCountGenerator(browser)) {
      totalPipCount += pipCount;
      totalPipDisabled += pipDisabledCount;
    }

    return { totalPipCount, totalPipDisabled };
  },

  /**
   * This function updates the hover text on the urlbar PiP button when we enter or exit PiP
   *
   * @param {Document} document The window document
   * @param {Element} pipToggle The urlbar PiP button
   * @param {string} dataL10nId The data l10n id of the string we want to show
   */
  updateUrlbarHoverText(document, pipToggle, dataL10nId) {
    let shortcut = document.getElementById("key_togglePictureInPicture");

    document.l10n.setAttributes(pipToggle, dataL10nId, {
      shortcut: ShortcutUtils.prettifyShortcut(shortcut),
    });
  },

  /**
   * Dispatch entry point used by the `browser-window-location-change`
   * category.
   */
  onLocationChange(_window, _locationURI, webProgress, _flags) {
    const browser = webProgress.browsingContext.embedderElement;
    if (!browser) {
      return;
    }
    this.updateUrlbarToggle(browser);
  },

  /**
   * Toggles the visibility of the PiP urlbar button. If the total video count
   * is 1, then we will show the button. If any eligible video has PiPDisabled,
   * then the button will show. Otherwise the button is hidden.
   *
   * @param {Browser} browser The selected browser
   */
  updateUrlbarToggle(browser) {
    if (!lazy.PIP_ENABLED || !lazy.PIP_URLBAR_BUTTON) {
      return;
    }

    // The browser may already be gone (e.g. tab closed) by the time the
    // async message that triggered this call is processed.
    if (!browser) {
      return;
    }

    let win = browser.documentGlobal;
    if (win.closed || win.gBrowser?.selectedBrowser !== browser) {
      return;
    }

    let { totalPipCount, totalPipDisabled } =
      this.getEligiblePipVideoCount(browser);

    let pipToggle = win.document.getElementById("picture-in-picture-button");
    if (
      totalPipCount === 1 ||
      (totalPipDisabled > 0 && lazy.RESPECT_PIP_DISABLED)
    ) {
      pipToggle.hidden = false;
      lazy.PageActions.sendPlacedInUrlbarTrigger(pipToggle);
    } else {
      pipToggle.hidden = true;
    }

    let browserHasPip = !!this.browserWeakMap.get(browser);
    if (browserHasPip) {
      this.setUrlbarPipIconActive(browser.documentGlobal);
    } else {
      this.setUrlbarPipIconInactive(browser.documentGlobal);
    }
  },

  /**
   * Open the PiP panel if any video has PiPDisabled, otherwise finds the
   * correct WindowGlobal to open the eligible PiP video.
   *
   * @param {Event} event Event from clicking the PiP urlbar button
   */
  toggleUrlbar(event) {
    let win = event.target.documentGlobal;
    let browser = win.gBrowser.selectedBrowser;

    let pipPanel = this.getPanelForBrowser(browser);

    for (let {
      windowGlobal,
      pipCount,
      pipDisabledCount,
    } of this.windowGlobalPipCountGenerator(browser)) {
      if (
        (pipDisabledCount > 0 && lazy.RESPECT_PIP_DISABLED) ||
        (pipPanel && pipPanel.state !== "closed")
      ) {
        this.togglePipPanel(browser);
        return;
      } else if (pipCount === 1) {
        let eventExtraKeys = {};
        if (
          !Services.prefs.getBoolPref(TOGGLE_HAS_USED_PREF) &&
          lazy.ASRouter.initialized
        ) {
          let { messages, messageImpressions } = lazy.ASRouter.state;
          let pipCallouts = messages.filter(
            message =>
              message.template === "feature_callout" &&
              message.content.screens.some(screen =>
                screen.anchors.some(anchor =>
                  anchor.selector.includes("picture-in-picture-button")
                )
              )
          );
          if (pipCallouts.length) {
            // Has one of the callouts been seen in the last 48 hours?
            let now = Date.now();
            let callout = pipCallouts.some(message =>
              messageImpressions[message.id]?.some(
                impression => now - impression < 48 * 60 * 60 * 1000
              )
            );
            if (callout) {
              eventExtraKeys.callout = true;
            }
          }
        }
        let actor = windowGlobal.getActor("PictureInPictureToggle");
        actor.sendAsyncMessage("PictureInPicture:UrlbarToggle", eventExtraKeys);
        return;
      }
    }
  },

  /**
   * Set the toggle for PiPDisabled when the panel is shown.
   * If the pref is set from about:config, we need to update
   * the toggle switch in the panel to match the pref.
   *
   * @param {Event} event The panel shown event
   */
  onPipPanelShown(event) {
    let toggle = event.target.querySelector("#respect-pipDisabled-switch");
    toggle.pressed = !lazy.RESPECT_PIP_DISABLED;
  },

  /**
   * Update the visibility of the urlbar PiP button when the panel is hidden.
   * The button will show when there is more than 1 video and at least 1 video
   * has PiPDisabled. If we no longer want to respect PiPDisabled then we
   * need to check if the urlbar button should still be visible.
   *
   * @param {Event} event The panel hidden event
   */
  onPipPanelHidden(event) {
    this.updateUrlbarToggle(event.view.gBrowser.selectedBrowser);
  },

  /**
   * Create the PiP panel if needed and toggle the display of the panel
   *
   * @param {Browser} browser The current browser
   */
  togglePipPanel(browser) {
    let pipPanel = this.getPanelForBrowser(browser);

    if (pipPanel.state === "closed") {
      let anchor = browser.ownerDocument.querySelector(
        "#picture-in-picture-button"
      );

      pipPanel.openPopup(anchor, "bottomright topright");
      Glean.pictureinpicture.openedMethodUrlBar.record({
        disableDialog: true,
      });
    } else {
      pipPanel.hidePopup();
    }
  },

  /**
   * Sets the PiP urlbar to an active state. This changes the icon in the
   * urlbar button to the unpip icon.
   *
   * @param {Window} win The current Window
   */
  setUrlbarPipIconActive(win) {
    let pipToggle = win.document.getElementById("picture-in-picture-button");
    pipToggle.toggleAttribute("pipactive", true);

    this.updateUrlbarHoverText(
      win.document,
      pipToggle,
      "picture-in-picture-urlbar-button-close"
    );
  },

  /**
   * Sets the PiP urlbar to an inactive state. This changes the icon in the
   * urlbar button to the open pip icon.
   *
   * @param {Window} win The current window
   */
  setUrlbarPipIconInactive(win) {
    if (!win) {
      return;
    }
    let pipToggle = win.document.getElementById("picture-in-picture-button");
    pipToggle.toggleAttribute("pipactive", false);

    this.updateUrlbarHoverText(
      win.document,
      pipToggle,
      "picture-in-picture-urlbar-button-open"
    );
  },

  /**
   * Remove attribute which enables pip icon in tab
   *
   * @param {Window} window
   *   A PictureInPicture player's window, used to resolve the player's
   *   associated originating content browser
   */
  clearPipTabIcon(window) {
    const browser = this.weakWinToBrowser.get(window);
    if (!browser) {
      return;
    }

    // see if no other pip windows are open for this content browser
    for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
      if (
        win !== window &&
        this.weakWinToBrowser.has(win) &&
        this.weakWinToBrowser.get(win) === browser
      ) {
        return;
      }
    }

    let gBrowser = browser.getTabBrowser();
    let tab = gBrowser?.getTabForBrowser(browser);
    if (tab) {
      tab.removeAttribute("pictureinpicture");
    }
  },

  /**
   * Closes and waits for passed PiP player window to finish closing.
   *
   * @param {Window} pipWin
   *   Player window to close
   */
  async closePipWindow(pipWin) {
    if (pipWin.closed) {
      return;
    }
    let closedPromise = new Promise(resolve => {
      pipWin.addEventListener("unload", resolve, { once: true });
    });
    pipWin.close();
    await closedPromise;
  },

  /**
   * Stops auto-toggle reopening a player the user closed while its window was
   * out of sight, until the window is shown again. The one-shot mark used for
   * background tabs can't do this: once closed, the player stops keeping the
   * window active, and the page reports hidden again, sometimes more than once.
   *
   * @param {Element} browser
   */
  suppressOccludedAutoPipUntilShown(browser) {
    if (this.weakOccludedBrowserClosedDeliberately.has(browser)) {
      return;
    }
    let win = browser.documentGlobal;
    this.weakOccludedBrowserClosedDeliberately.add(browser);
    let onWindowStateChange = () => {
      if (isWindowOutOfSight(win)) {
        return;
      }
      win.removeEventListener("sizemodechange", onWindowStateChange);
      win.removeEventListener("occlusionstatechange", onWindowStateChange);
      this.weakOccludedBrowserClosedDeliberately.delete(browser);
    };
    win.addEventListener("sizemodechange", onWindowStateChange);
    win.addEventListener("occlusionstatechange", onWindowStateChange);
  },

  /**
   * Closes a single PiP window. Used exclusively in conjunction with support
   * for multiple PiP windows
   *
   * @param {object} closeData
   *   Additional data required to complete a close operation on a PiP window
   * @param {PictureInPictureParent} closeData.actorRef
   *   The PictureInPictureParent actor associated with the PiP window being closed
   * @param {string} closeData.reason
   *   The reason for closing this PiP window
   */
  async closeSinglePipWindow(closeData) {
    const { reason, actorRef } = closeData;
    const win = this.getWeakPipPlayer(actorRef);
    if (!win) {
      return;
    }

    const browser = this.weakWinToBrowser.get(win);

    if (browser && DELIBERATE_CLOSE_REASONS.includes(reason)) {
      const tabbrowser = browser.getTabBrowser();
      if (tabbrowser && tabbrowser.selectedBrowser != browser) {
        PictureInPicture.weakAutoPipBrowserClosedDeliberately.add(browser);
      } else if (
        tabbrowser &&
        lazy.PIP_WHEN_OCCLUDED &&
        isWindowOutOfSight(browser.documentGlobal)
      ) {
        this.suppressOccludedAutoPipUntilShown(browser);
      }
    }

    this.removePiPBrowserFromWeakMap(this.weakWinToBrowser.get(win));
    this.weakAutoPipBrowserToParent.delete(this.weakWinToBrowser.get(win));

    Glean.pictureinpicture["closedMethod" + reason].record();
    await this.closePipWindow(win);
  },

  /**
   * Set the window that was created from the Picture In Picture API.
   * Used specifically to force-close if another PIP API request is made.
   */
  setApiWindow(window) {
    if (this.apiPipWindow != null) {
      console.error(`PIP API Window reference not properly cleared.`);
    }
    this.apiPipWindow = Cu.getWeakReference(window);
    window.addEventListener("unload", () => {
      this.clearApiWindow(window);
    });
  },

  /**
   * When a PIP Window (PIP API) unloads it clears the weak reference to itself.
   */
  clearApiWindow(window) {
    let currentWindow = this.apiPipWindow?.get();
    if (currentWindow == window) {
      this.apiPipWindow = null;
    } else {
      console.error(`PIP API Window state not properly cleared.`);
    }
  },

  /**
   * Closes PIP window that was opened via the Picture-in-Picture API.
   * Current PIP API implementation has chosen to only support 1 window at a time.
   *
   * @param {string} reason
   *   The reason for closing these windows (for telemetry)
   */
  async closeApiPipWindowIfOpen(reason = "Api") {
    const pipApiWindow = this.apiPipWindow?.get();
    if (pipApiWindow) {
      Glean.pictureinpicture["closedMethod" + reason].record();
      await this.closePipWindow(pipApiWindow);
      this.apiPipWindow = null;
    }
  },

  /**
   * A request has come up from content to open a Picture in Picture
   * window.
   *
   * @param {WindowGlobalParent} wgps
   *   The WindowGlobalParent that is requesting the Picture in Picture
   *   window.
   *
   * @param {object} videoData
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   *   videoRef (ContentDOMReference)
   *    A reference to the video element that a Picture-in-Picture window
   *    is being created for
   *
   *   isPipApiRequest {boolean}
   *    True when this request originated from HTMLVideoElement.requestPictureInPicture().
   *    The PictureInPictureWindow instance itself is handed off to the player
   *    actor via a same-process WeakMap in PictureInPictureChild.sys.mjs.
   *
   * @returns {Promise}
   *   Resolves once the Picture in Picture window has been created, and
   *   the player component inside it has finished loading.
   */
  async handlePictureInPictureRequest(wgp, videoData) {
    const isApiRequest = !!videoData.isPipApiRequest;

    if (isApiRequest) {
      await this.closeApiPipWindowIfOpen();
    }

    this.currentPlayerCount += 1;
    this.maxConcurrentPlayerCount = Math.max(
      this.maxConcurrentPlayerCount,
      this.currentPlayerCount
    );
    Glean.pictureinpicture.mostConcurrentPlayers.set(
      this.maxConcurrentPlayerCount
    );

    let browser = wgp.browsingContext.top.embedderElement;
    let parentWin = browser.documentGlobal;

    let win = await this.openPipWindow(parentWin, videoData);
    win.setIsPlayingState(videoData.playing);
    win.setIsMutedState(videoData.isMuted);

    // set attribute which shows pip icon in tab
    let tab = parentWin.gBrowser.getTabForBrowser(browser);
    tab.setAttribute("pictureinpicture", true);

    this.setUrlbarPipIconActive(parentWin);

    tab.addEventListener("TabSwapPictureInPicture", this);

    let pipId = gNextWindowID.toString();
    const { actor: actorRef, setupPromise } = win.setupPlayer(
      pipId,
      wgp,
      videoData.videoRef,
      isApiRequest,
      videoData.autoFocus
    );
    gNextWindowID++;

    this.weakWinToBrowser.set(win, browser);
    this.addPiPBrowserToWeakMap(browser);
    this.addOriginatingWinToWeakMap(browser);
    if (
      (lazy.PIP_WHEN_SWITCHING_TABS || lazy.PIP_WHEN_OCCLUDED) &&
      !browser.docShellIsActive
    ) {
      // The docshell would only not be active when the video was pip'd via auto toggle
      browser.docShellIsActive = true;
      this.weakAutoPipBrowserToParent.set(browser, actorRef);
    }

    if (isApiRequest) {
      this.setApiWindow(win);
    }

    win.setScrubberPosition(videoData.scrubberPosition);
    win.setTimestamp(videoData.timestamp);
    win.setVolume(videoData.volume);
    win.setPlaybackRate(videoData.playbackRate);

    Services.prefs.setBoolPref(TOGGLE_HAS_USED_PREF, true);

    Glean.pictureinpicture.createPlayer.record({
      value: pipId,
      width: win.innerWidth,
      height: win.innerHeight,
      screenX: win.screenX,
      screenY: win.screenY,
      ccEnabled: videoData.ccEnabled,
      webVTTSubtitles: videoData.webVTTSubtitles,
    });
    await setupPromise;
  },

  /**
   * Calls the browsingContext's `forceAppWindowActive` flag to determine if the
   * the top level chrome browsingContext should be forcefully set as active or not.
   * When the originating window's browsing context is set to active, captions on the
   * PiP window are properly updated. Forcing active while a PiP window is open ensures
   * that captions are still updated when the originating window is occluded.
   *
   * @param {BrowsingContext} browsingContext
   *   The browsing context of the originating window
   * @param {boolean} isActive
   *   True to force originating window as active, or false to not enforce it
   * @see CanonicalBrowsingContext
   */
  setOriginatingWindowActive(browsingContext, isActive) {
    browsingContext.forceAppWindowActive = isActive;
  },

  /**
   * unload event has been called in player.js, cleanup our preserved
   * browser object.
   *
   * @param {Window} window
   */
  unload(window) {
    Glean.pictureinpicture.windowOpenDuration.stopAndAccumulate(
      window._openDurationTimerId
    );

    if (window._backgroundTabTimerId) {
      Glean.pictureinpicture.backgroundTabPlayingDuration.stopAndAccumulate(
        window._backgroundTabTimerId
      );
      window._backgroundTabTimerId = null;
    } else if (window._foregroundTabTimerId) {
      Glean.pictureinpicture.foregroundTabPlayingDuration.stopAndAccumulate(
        window._foregroundTabTimerId
      );
      window._foregroundTabTimerId = null;
    }

    let browser = this.weakWinToBrowser.get(window);
    this.removeOriginatingWinFromWeakMap(browser);

    this.currentPlayerCount -= 1;
    // Saves the location of the Picture in Picture window
    this.savePosition(window);
    this.clearPipTabIcon(window);
    this.setUrlbarPipIconInactive(browser?.documentGlobal);
  },

  /**
   * Open a Picture in Picture window on the same screen as parentWin,
   * sized based on the information in videoData.
   *
   * @param {ChromeWindow} parentWin
   *   The window hosting the browser that requested the Picture in
   *   Picture window.
   *
   * @param {object} videoData
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   * @param {PictureInPictureParent} actorReference
   *   Reference to the calling PictureInPictureParent
   *
   * @returns {Promise}
   *   Resolves once the window has opened and loaded the player component.
   */
  async openPipWindow(parentWin, videoData) {
    let { top, left, width, height } = this.fitToScreen(parentWin, videoData);

    let { left: resolvedLeft, top: resolvedTop } = this.resolveOverlapConflicts(
      left,
      top,
      width,
      height
    );

    top = Math.round(resolvedTop);
    left = Math.round(resolvedLeft);
    width = Math.round(width);
    height = Math.round(height);

    let features =
      `${PLAYER_FEATURES},top=${top},left=${left},outerWidth=${width},` +
      `outerHeight=${height}`;
    let isPrivate = lazy.PrivateBrowsingUtils.isWindowPrivate(parentWin);

    if (isPrivate) {
      features += ",private";
    }

    let pipWindow = Services.ww.openWindow(
      parentWin,
      PLAYER_URI,
      null,
      features,
      null
    );

    pipWindow._openDurationTimerId =
      Glean.pictureinpicture.windowOpenDuration.start();

    pipWindow.windowUtils.setResizeMargin(RESIZE_MARGIN_PX);

    // If the window is Private the icon will have already been set when
    // it was opened.
    if (Services.appinfo.OS == "WINNT" && !isPrivate) {
      lazy.WindowsUIUtils.setWindowIconNoData(pipWindow);
    }

    return new Promise(resolve => {
      pipWindow.addEventListener(
        "load",
        () => {
          resolve(pipWindow);
        },
        { once: true }
      );
    });
  },

  /**
   * This function tries to restore the last known Picture-in-Picture location
   * and size. If those values are unknown or offscreen, then a default
   * location and size is used.
   *
   * @param {ChromeWindow|PlayerWindow} requestingWin
   *   The window hosting the browser that requested the Picture in
   *   Picture window. If this is an existing player window then the returned
   *   player size and position will be determined based on the existing
   *   player window's size and position.
   *
   * @param {object} videoData
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   * @returns {object}
   *   The size and position for the player window, in CSS pixels relative to
   *   requestingWin.
   *
   *   top (int):
   *     The top position for the player window.
   *
   *   left (int):
   *     The left position for the player window.
   *
   *   width (int):
   *     The width of the player window.
   *
   *   height (int):
   *     The height of the player window.
   */
  fitToScreen(requestingWin, videoData) {
    let { videoHeight, videoWidth } = videoData;

    const isPlayer = requestingWin.document.location.href == PLAYER_URI;

    let requestingCssToDesktopScale =
      requestingWin.devicePixelRatio / requestingWin.desktopToDeviceScale;

    let top, left, width, height;
    if (!isPlayer) {
      // requestingWin is a content window, load last PiP's dimensions
      ({ top, left, width, height } = this.loadPosition());
    } else if (requestingWin.windowState === requestingWin.STATE_FULLSCREEN) {
      // `requestingWin` is a PiP window and in fullscreen. We stored the size
      // and position before entering fullscreen and we will use that to
      // calculate the new position
      ({ top, left, width, height } = requestingWin.getDeferredResize());
      left *= requestingCssToDesktopScale;
      top *= requestingCssToDesktopScale;
    } else {
      // requestingWin is a PiP player, conserve its dimensions in this case
      left = requestingWin.screenX * requestingCssToDesktopScale;
      top = requestingWin.screenY * requestingCssToDesktopScale;
      width = requestingWin.outerWidth;
      height = requestingWin.outerHeight;
    }

    // Check that previous location and size were loaded.
    // Note that at this point left and top are in desktop pixels, while width
    // and height are in CSS pixels.
    if (!isNaN(top) && !isNaN(left) && !isNaN(width) && !isNaN(height)) {
      // Get the screen of the last PiP window. PiP screen will be the default
      // screen if the point was not on a screen.
      let PiPScreen = this.getWorkingScreen(left, top);

      // Center position of PiP window.
      let PipScreenCssToDesktopScale =
        PiPScreen.defaultCSSScaleFactor / PiPScreen.contentsScaleFactor;
      let centerX = left + (width * PipScreenCssToDesktopScale) / 2;
      let centerY = top + (height * PipScreenCssToDesktopScale) / 2;

      // We have the screen, now we will get the dimensions of the screen
      let [PiPScreenLeft, PiPScreenTop, PiPScreenWidth, PiPScreenHeight] =
        this.getAvailScreenSize(PiPScreen);

      // Check that the center of the last PiP location is within the screen limits
      // If it's not, then we will use the default size and position
      if (
        PiPScreenLeft <= centerX &&
        centerX <= PiPScreenLeft + PiPScreenWidth &&
        PiPScreenTop <= centerY &&
        centerY <= PiPScreenTop + PiPScreenHeight
      ) {
        let oldWidthDesktopPix = width * PipScreenCssToDesktopScale;

        // The new PiP window will keep the height of the old
        // PiP window and adjust the width to the correct ratio
        width = Math.round((height * videoWidth) / videoHeight);

        // Minimum window size on Windows is 136
        if (AppConstants.platform == "win") {
          width = 136 > width ? 136 : width;
        }

        let widthDesktopPix = width * PipScreenCssToDesktopScale;
        let heightDesktopPix = height * PipScreenCssToDesktopScale;

        // WIGGLE_ROOM allows the PiP window to be within 5 pixels of the right
        // side of the screen to stay snapped to the right side
        const WIGGLE_ROOM = 5;
        // If the PiP window was right next to the right side of the screen
        // then move the PiP window to the right the same distance that
        // the width changes from previous width to current width
        let rightScreen = PiPScreenLeft + PiPScreenWidth;
        let distFromRight = rightScreen - (left + widthDesktopPix);
        if (
          0 < distFromRight &&
          distFromRight <= WIGGLE_ROOM + (oldWidthDesktopPix - widthDesktopPix)
        ) {
          left += distFromRight;
        }

        // Checks if some of the PiP window is off screen and
        // if so it will adjust to move everything on screen
        if (left < PiPScreenLeft) {
          // off the left of the screen
          // slide right
          left = PiPScreenLeft;
        }
        if (top < PiPScreenTop) {
          // off the top of the screen
          // slide down
          top = PiPScreenTop;
        }
        if (left + widthDesktopPix > PiPScreenLeft + PiPScreenWidth) {
          // off the right of the screen
          // slide left
          left = PiPScreenLeft + PiPScreenWidth - widthDesktopPix;
        }
        if (top + heightDesktopPix > PiPScreenTop + PiPScreenHeight) {
          // off the bottom of the screen
          // slide up
          top = PiPScreenTop + PiPScreenHeight - heightDesktopPix;
        }
        // Convert top / left from desktop to requestingWin-relative CSS pixels.
        top /= requestingCssToDesktopScale;
        left /= requestingCssToDesktopScale;
        return { top, left, width, height };
      }
    }

    // We don't have the size or position of the last PiP window, so fall
    // back to calculating the default location.
    let screen = this.getWorkingScreen(
      requestingWin.screenX * requestingCssToDesktopScale,
      requestingWin.screenY * requestingCssToDesktopScale,
      requestingWin.outerWidth * requestingCssToDesktopScale,
      requestingWin.outerHeight * requestingCssToDesktopScale
    );
    let [screenLeft, screenTop, screenWidth, screenHeight] =
      this.getAvailScreenSize(screen);

    let screenCssToDesktopScale =
      screen.defaultCSSScaleFactor / screen.contentsScaleFactor;

    // The Picture in Picture window will be a maximum of a quarter of
    // the screen height, and a third of the screen width.
    const MAX_HEIGHT = screenHeight / 4;
    const MAX_WIDTH = screenWidth / 3;

    width = videoWidth * screenCssToDesktopScale;
    height = videoHeight * screenCssToDesktopScale;
    let aspectRatio = videoWidth / videoHeight;

    if (videoHeight > MAX_HEIGHT || videoWidth > MAX_WIDTH) {
      // We're bigger than the max.
      // Take the largest dimension and clamp it to the associated max.
      // Recalculate the other dimension to maintain aspect ratio.
      if (videoWidth >= videoHeight) {
        // We're clamping the width, so the height must be adjusted to match
        // the original aspect ratio. Since aspect ratio is width over height,
        // that means we need to _divide_ the MAX_WIDTH by the aspect ratio to
        // calculate the appropriate height.
        width = MAX_WIDTH;
        height = Math.round(MAX_WIDTH / aspectRatio);
      } else {
        // We're clamping the height, so the width must be adjusted to match
        // the original aspect ratio. Since aspect ratio is width over height,
        // this means we need to _multiply_ the MAX_HEIGHT by the aspect ratio
        // to calculate the appropriate width.
        height = MAX_HEIGHT;
        width = Math.round(MAX_HEIGHT * aspectRatio);
      }
    }

    // Now that we have the dimensions of the video, we need to figure out how
    // to position it in the bottom right corner. Since we know the width of the
    // available rect, we need to subtract the dimensions of the window we're
    // opening to get the top left coordinates that openWindow expects.
    //
    // In event that the user has multiple displays connected, we have to
    // calculate the top-left coordinate of the new window in absolute
    // coordinates that span the entire display space, since this is what the
    // openWindow expects for its top and left feature values.
    //
    // The screenWidth and screenHeight values only tell us the available
    // dimensions on the screen that the parent window is on. We add these to
    // the screenLeft and screenTop values, which tell us where this screen is
    // located relative to the "origin" in absolute coordinates.
    let isRTL = Services.locale.isAppLocaleRTL;
    left = isRTL ? screenLeft : screenLeft + screenWidth - width;
    top = screenTop + screenHeight - height;

    // Convert top/left from desktop pixels to requestingWin-relative CSS
    // pixels, and width / height to the target screen's CSS pixels, which is
    // what we've made the size calculation against.
    top /= requestingCssToDesktopScale;
    left /= requestingCssToDesktopScale;
    width /= screenCssToDesktopScale;
    height /= screenCssToDesktopScale;

    return { top, left, width, height };
  },

  /**
   * This function will take the size and potential location of a new
   * Picture-in-Picture player window, and try to return the location
   * coordinates that will best ensure that the player window will not overlap
   * with other pre-existing player windows.
   *
   * @param {int} left
   *  x position of left edge for Picture-in-Picture window that is being
   *  opened
   * @param {int} top
   *  y position of top edge for Picture-in-Picture window that is being
   *  opened
   * @param {int} width
   *  Width of Picture-in-Picture window that is being opened
   * @param {int} height
   *  Height of Picture-in-Picture window that is being opened
   *
   * @returns {object}
   *  An object with the following properties:
   *
   *   top (int):
   *     The recommended top position for the player window.
   *
   *   left (int):
   *     The recommended left position for the player window.
   */
  resolveOverlapConflicts(left, top, width, height) {
    // This algorithm works by first identifying the possible candidate
    // locations that the new player window could be placed without overlapping
    // other player windows (assuming that a confict is discovered at all of
    // course). The optimal candidate is then selected by its distance to the
    // original conflict, shorter distances are better.
    //
    // Candidates are discovered by iterating over each of the sides of every
    // pre-existing player window. One candidate is collected for each side.
    // This is done to ensure that the new player window will be opened to
    // tightly fit along the edge of another player window.
    //
    // These candidates are then pruned for candidates that will introduce
    // further conflicts. Finally the ideal candidate is selected from this
    // pool of remaining candidates, optimized for minimizing distance to
    // the original conflict.
    let playerRects = [];

    for (let playerWin of Services.wm.getEnumerator(WINDOW_TYPE)) {
      playerRects.push(
        new Rect(
          playerWin.screenX,
          playerWin.screenY,
          playerWin.outerWidth,
          playerWin.outerHeight
        )
      );
    }

    const newPlayerRect = new Rect(left, top, width, height);
    let conflictingPipRect = playerRects.find(rect =>
      rect.intersects(newPlayerRect)
    );

    if (!conflictingPipRect) {
      // no conflicts found
      return { left, top };
    }

    const conflictLoc = conflictingPipRect.center();

    // Will try to resolve a better placement only on the screen where
    // the conflict occurred
    const conflictScreen = this.getWorkingScreen(conflictLoc.x, conflictLoc.y);

    const [screenTop, screenLeft, screenWidth, screenHeight] =
      this.getAvailScreenSize(conflictScreen);

    const screenRect = new Rect(
      screenTop,
      screenLeft,
      screenWidth,
      screenHeight
    );

    const getEdgeCandidates = rect => {
      return [
        // left edge's candidate
        new Point(rect.left - newPlayerRect.width, rect.top),
        // top edge's candidate
        new Point(rect.left, rect.top - newPlayerRect.height),
        // right edge's candidate
        new Point(rect.right + newPlayerRect.width, rect.top),
        // bottom edge's candidate
        new Point(rect.left, rect.bottom),
      ];
    };

    let candidateLocations = [];
    for (const playerRect of playerRects) {
      for (let candidateLoc of getEdgeCandidates(playerRect)) {
        const candidateRect = new Rect(
          candidateLoc.x,
          candidateLoc.y,
          width,
          height
        );

        if (!screenRect.contains(candidateRect)) {
          continue;
        }

        // test that no PiPs conflict with this candidate box
        if (playerRects.some(rect => rect.intersects(candidateRect))) {
          continue;
        }

        const candidateCenter = candidateRect.center();
        const candidateDistanceToConflict =
          Math.abs(conflictLoc.x - candidateCenter.x) +
          Math.abs(conflictLoc.y - candidateCenter.y);

        candidateLocations.push({
          distanceToConflict: candidateDistanceToConflict,
          location: candidateLoc,
        });
      }
    }

    if (!candidateLocations.length) {
      // if no suitable candidates can be found, return the original location
      return { left, top };
    }

    // sort candidates by distance to the conflict, select the closest
    const closestCandidate = candidateLocations.sort(
      (firstCand, secondCand) =>
        firstCand.distanceToConflict - secondCand.distanceToConflict
    )[0];

    if (!closestCandidate) {
      // can occur if there were no valid candidates, return original location
      return { left, top };
    }

    const resolvedX = closestCandidate.location.x;
    const resolvedY = closestCandidate.location.y;

    return { left: resolvedX, top: resolvedY };
  },

  /**
   * Resizes the the PictureInPicture player window.
   *
   * @param {object} videoData
   *    The source video's data.
   * @param {PictureInPictureParent} actorRef
   *    Reference to the PictureInPicture parent actor.
   */
  resizePictureInPictureWindow(videoData, actorRef) {
    let win = this.getWeakPipPlayer(actorRef);

    if (!win) {
      return;
    }

    win.resizeToVideo(this.fitToScreen(win, videoData));
  },

  /**
   * Opens the context menu for toggling PictureInPicture.
   *
   * @param {Window} window
   * @param {object} data
   *  Message data from the PictureInPictureToggleParent
   */
  openToggleContextMenu(window, data) {
    let document = window.document;
    let popup = document.getElementById("pictureInPictureToggleContextMenu");
    let contextMoveToggle = document.getElementById(
      "context_MovePictureInPictureToggle"
    );

    // Set directional string for toggle position
    let position = Services.prefs.getStringPref(
      TOGGLE_POSITION_PREF,
      TOGGLE_POSITION_RIGHT
    );
    switch (position) {
      case TOGGLE_POSITION_RIGHT:
        document.l10n.setAttributes(
          contextMoveToggle,
          "picture-in-picture-move-toggle-left"
        );
        break;
      case TOGGLE_POSITION_LEFT:
        document.l10n.setAttributes(
          contextMoveToggle,
          "picture-in-picture-move-toggle-right"
        );
        break;
    }

    // We synthesize a new MouseEvent to propagate the inputSource to the
    // subsequently triggered popupshowing event.
    let newEvent = new PointerEvent("contextmenu", {
      bubbles: true,
      cancelable: true,
      screenX: data.screenXDevPx / window.devicePixelRatio,
      screenY: data.screenYDevPx / window.devicePixelRatio,
      pointerType: (() => {
        switch (data.inputSource) {
          case MouseEvent.MOZ_SOURCE_MOUSE:
            return "mouse";
          case MouseEvent.MOZ_SOURCE_PEN:
            return "pen";
          case MouseEvent.MOZ_SOURCE_ERASER:
            return "eraser";
          case MouseEvent.MOZ_SOURCE_CURSOR:
            return "cursor";
          case MouseEvent.MOZ_SOURCE_TOUCH:
            return "touch";
          case MouseEvent.MOZ_SOURCE_KEYBOARD:
            return "keyboard";
          default:
            return "";
        }
      })(),
    });
    popup.openPopupAtScreen(newEvent.screenX, newEvent.screenY, true, newEvent);
  },

  hideToggle() {
    Services.prefs.setBoolPref(TOGGLE_ENABLED_PREF, false);
    Glean.pictureinpictureSettings.disablePlayer.record();
  },

  /**
   * This is used in AsyncTabSwitcher.sys.mjs and tabbrowser.js to check if the browser
   * currently has a PiP window.
   * If the browser has a PiP window we want to keep the browser in an active state because
   * the browser is still partially visible.
   *
   * @param browser The browser to check if it has a PiP window
   * @returns true if browser has PiP window else false
   */
  isOriginatingBrowser(browser) {
    return this.browserWeakMap.has(browser);
  },

  moveToggle() {
    // Get the current position
    let position = Services.prefs.getStringPref(
      TOGGLE_POSITION_PREF,
      TOGGLE_POSITION_RIGHT
    );
    let newPosition = "";
    // Determine what the opposite position would be for that preference
    switch (position) {
      case TOGGLE_POSITION_RIGHT:
        newPosition = TOGGLE_POSITION_LEFT;
        break;
      case TOGGLE_POSITION_LEFT:
        newPosition = TOGGLE_POSITION_RIGHT;
        break;
    }
    if (newPosition) {
      Services.prefs.setStringPref(TOGGLE_POSITION_PREF, newPosition);
    }
  },

  /**
   * This function takes a screen and will return the left, top, width and
   * height of the screen
   *
   * @param {Screen} screen
   * The screen we need to get the size and coordinates of
   *
   * @returns {Array}
   * Size and location of screen in desktop pixels.
   *
   *   screenLeft.value (int):
   *     The left position for the screen.
   *
   *   screenTop.value (int):
   *     The top position for the screen.
   *
   *   screenWidth.value (int):
   *     The width of the screen.
   *
   *   screenHeight.value (int):
   *     The height of the screen.
   */
  getAvailScreenSize(screen) {
    let screenLeft = {},
      screenTop = {},
      screenWidth = {},
      screenHeight = {};
    screen.GetAvailRectDisplayPix(
      screenLeft,
      screenTop,
      screenWidth,
      screenHeight
    );
    return [
      screenLeft.value,
      screenTop.value,
      screenWidth.value,
      screenHeight.value,
    ];
  },

  /**
   * This function takes in a rect in desktop pixels, and returns the screen it
   * is located on.
   *
   * If the left and top are not on any screen, it will return the default
   * screen.
   *
   * @param {int} left
   *  left or x coordinate
   *
   * @param {int} top
   *  top or y coordinate
   *
   * @param {int} width
   *  top or y coordinate
   *
   * @param {int} height
   *  top or y coordinate
   *
   * @returns {Screen} screen
   *  the screen the left and top are on otherwise, default screen
   */
  getWorkingScreen(left, top, width = 1, height = 1) {
    // Get the screen manager
    let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
      Ci.nsIScreenManager
    );
    // use screenForRect to get screen
    // this returns the default screen if left and top are not
    // on any screen
    return screenManager.screenForRect(left, top, width, height);
  },

  /**
   * Saves position and size of Picture-in-Picture window
   *
   * @param {Window} win The Picture-in-Picture window
   */
  savePosition(win) {
    let xulStore = Services.xulStore;

    // We store left / top position in desktop pixels, like SessionStore does,
    // so that we can restore them properly (as CSS pixels need to be relative
    // to a screen, and we won't have a target screen to restore).
    let cssToDesktopScale = win.devicePixelRatio / win.desktopToDeviceScale;

    let left = win.screenX * cssToDesktopScale;
    let top = win.screenY * cssToDesktopScale;
    let width = win.outerWidth;
    let height = win.outerHeight;

    xulStore.setValue(PLAYER_URI, "picture-in-picture", "left", left);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "top", top);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "width", width);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "height", height);
  },

  /**
   * Load last Picture in Picture location and size
   *
   * @returns {object}
   *   The size and position of the last Picture in Picture window.
   *
   *   top (int):
   *     The top position for the last player window.
   *     Otherwise NaN
   *
   *   left (int):
   *     The left position for the last player window.
   *     Otherwise NaN
   *
   *   width (int):
   *     The width of the player last window.
   *     Otherwise NaN
   *
   *   height (int):
   *     The height of the player last window.
   *     Otherwise NaN
   */
  loadPosition() {
    let xulStore = Services.xulStore;

    let left = parseInt(
      xulStore.getValue(PLAYER_URI, "picture-in-picture", "left")
    );
    let top = parseInt(
      xulStore.getValue(PLAYER_URI, "picture-in-picture", "top")
    );
    let width = parseInt(
      xulStore.getValue(PLAYER_URI, "picture-in-picture", "width")
    );
    let height = parseInt(
      xulStore.getValue(PLAYER_URI, "picture-in-picture", "height")
    );

    return { top, left, width, height };
  },

  setFirstSeen(dateSeconds) {
    if (!dateSeconds) {
      return;
    }

    Services.prefs.setIntPref(TOGGLE_FIRST_SEEN_PREF, dateSeconds);
  },

  setHasUsed(hasUsed) {
    Services.prefs.setBoolPref(TOGGLE_HAS_USED_PREF, !!hasUsed);
  },
};
