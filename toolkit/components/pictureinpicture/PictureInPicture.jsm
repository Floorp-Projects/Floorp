/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "PictureInPicture",
  "PictureInPictureParent",
  "PictureInPictureToggleParent",
  "PictureInPictureLauncherParent",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const PLAYER_URI = "chrome://global/content/pictureinpicture/player.xhtml";
var PLAYER_FEATURES =
  "chrome,titlebar=yes,alwaysontop,lockaspectratio,resizable";
/* Don't use dialog on Gtk as it adds extra border and titlebar to PIP window */
if (!AppConstants.MOZ_WIDGET_GTK) {
  PLAYER_FEATURES += ",dialog";
}
const WINDOW_TYPE = "Toolkit:PictureInPicture";
const PIP_ENABLED_PREF = "media.videocontrols.picture-in-picture.enabled";
const MULTI_PIP_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.allow-multiple";
const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";

/**
 * If closing the Picture-in-Picture player window occurred for a reason that
 * we can easily detect (user clicked on the close button, originating tab unloaded,
 * user clicked on the unpip button), that will be stashed in gCloseReasons so that
 * we can note it in Telemetry when the window finally unloads.
 */
let gCloseReasons = new WeakMap();

/**
 * Tracks the number of currently open player windows for Telemetry tracking
 */
let gCurrentPlayerCount = 0;

/**
 * To differentiate windows in the Telemetry Event Log, each Picture-in-Picture
 * player window is given a unique ID.
 */
let gNextWindowID = 0;

class PictureInPictureLauncherParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "PictureInPicture:Request": {
        let videoData = aMessage.data;
        PictureInPicture.handlePictureInPictureRequest(this.manager, videoData);
        break;
      }
    }
  }
}

class PictureInPictureToggleParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    let browsingContext = aMessage.target.browsingContext;
    let browser = browsingContext.top.embedderElement;
    switch (aMessage.name) {
      case "PictureInPicture:OpenToggleContextMenu": {
        let win = browser.ownerGlobal;
        PictureInPicture.openToggleContextMenu(win, aMessage.data);
        break;
      }
    }
  }
}

/**
 * This module is responsible for creating a Picture in Picture window to host
 * a clone of a video element running in web content.
 */
class PictureInPictureParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
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

        if (PictureInPicture.isMultiPipEnabled) {
          PictureInPicture.closeSinglePipWindow({ reason, actorRef: this });
        } else {
          PictureInPicture.closeAllPipWindows({ reason });
        }
        break;
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
    }
  }
}

/**
 * This module is responsible for creating a Picture in Picture window to host
 * a clone of a video element running in web content.
 */

var PictureInPicture = {
  // Maps PictureInPictureParent actors to their corresponding PiP player windows
  weakPipToWin: new WeakMap(),

  // Maps PiP player windows to their originating content's browser
  weakWinToBrowser: new WeakMap(),

  /**
   * Returns the player window if one exists and if it hasn't yet been closed.
   *
   * @param pipActorRef (PictureInPictureParent)
   * 	Reference to the calling PictureInPictureParent actor
   *
   * @return {DOM Window} the player window if it exists and is not in the
   * process of being closed. Returns null otherwise.
   */
  getWeakPipPlayer(pipActorRef) {
    let playerWin = this.weakPipToWin.get(pipActorRef);
    if (!playerWin || playerWin.closed) {
      return null;
    }
    return playerWin;
  },

  handleEvent(event) {
    switch (event.type) {
      case "TabSwapPictureInPicture": {
        this.onPipSwappedBrowsers(event);
      }
    }
  },

  onPipSwappedBrowsers(event) {
    let otherTab = event.detail;
    if (otherTab) {
      for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
        if (this.weakWinToBrowser.get(win) === event.target.linkedBrowser) {
          this.weakWinToBrowser.set(win, otherTab.linkedBrowser);
        }
      }
      otherTab.addEventListener("TabSwapPictureInPicture", this);
    }
  },

  /**
   * Called when the browser UI handles the View:PictureInPicture command via
   * the keyboard.
   */
  onCommand(event) {
    if (!Services.prefs.getBoolPref(PIP_ENABLED_PREF, false)) {
      return;
    }

    let win = event.target.ownerGlobal;
    let browser = win.gBrowser.selectedBrowser;
    let actor = browser.browsingContext.currentWindowGlobal.getActor(
      "PictureInPictureLauncher"
    );
    actor.sendAsyncMessage("PictureInPicture:KeyToggle");
  },

  async focusTabAndClosePip(window, pipActor) {
    let browser = this.weakWinToBrowser.get(window);
    if (!browser) {
      return;
    }

    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);

    gBrowser.selectedTab = tab;
    await this.closeSinglePipWindow({ reason: "unpip", actorRef: pipActor });
  },

  /**
   * Remove attribute which enables pip icon in tab
   *
   * @param window {Window}
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

    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
    if (tab) {
      tab.removeAttribute("pictureinpicture");
    }
  },

  /**
   * Closes and waits for passed PiP player window to finish closing.
   *
   * @param pipWin {Window}
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
   * Closes a single PiP window. Used exclusively in conjunction with support
   * for multiple PiP windows
   *
   * @param {Object} closeData
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

    await this.closePipWindow(win);
    gCloseReasons.set(win, reason);
  },

  /**
   * Find and close any pre-existing Picture in Picture windows. Used exclusively
   * when multiple PiP window support is turned off. All windows can be closed because it
   * is assumed that only 1 window is open when it is called.
   *
   * @param {Object} closeData
   *   Additional data required to complete a close operation on a PiP window
   * @param {string} closeData.reason
   *   The reason why this PiP is being closed
   */
  async closeAllPipWindows(closeData) {
    const { reason } = closeData;

    // This uses an enumerator, but there really should only be one of
    // these things.
    for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
      if (win.closed) {
        continue;
      }
      let closedPromise = new Promise(resolve => {
        win.addEventListener("unload", resolve, { once: true });
      });
      gCloseReasons.set(win, reason);
      win.close();
      await closedPromise;
    }
  },

  /**
   * A request has come up from content to open a Picture in Picture
   * window.
   *
   * @param wgp (WindowGlobalParent)
   *   The WindowGlobalParent that is requesting the Picture in Picture
   *   window.
   *
   * @param videoData (object)
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   * @returns Promise
   *   Resolves once the Picture in Picture window has been created, and
   *   the player component inside it has finished loading.
   */
  async handlePictureInPictureRequest(wgp, videoData) {
    if (!this.isMultiPipEnabled) {
      // If there's a pre-existing PiP window, close it first if multiple
      // pips are disabled
      await this.closeAllPipWindows({ reason: "new-pip" });

      gCurrentPlayerCount = 1;
    } else {
      // track specific number of open pip players if multi pip is
      // enabled

      gCurrentPlayerCount += 1;
    }

    Services.telemetry.scalarSetMaximum(
      "pictureinpicture.most_concurrent_players",
      gCurrentPlayerCount
    );

    let browser = wgp.browsingContext.top.embedderElement;
    let parentWin = browser.ownerGlobal;

    let win = await this.openPipWindow(parentWin, videoData);
    win.setIsPlayingState(videoData.playing);
    win.setIsMutedState(videoData.isMuted);

    // set attribute which shows pip icon in tab
    let tab = parentWin.gBrowser.getTabForBrowser(browser);
    tab.setAttribute("pictureinpicture", true);

    tab.addEventListener("TabSwapPictureInPicture", this);

    win.setupPlayer(gNextWindowID.toString(), wgp, videoData.videoRef);
    gNextWindowID++;

    this.weakWinToBrowser.set(win, browser);

    Services.prefs.setBoolPref(
      "media.videocontrols.picture-in-picture.video-toggle.has-used",
      true
    );
  },

  /**
   * unload event has been called in player.js, cleanup our preserved
   * browser object.
   */
  unload(window) {
    TelemetryStopwatch.finish(
      "FX_PICTURE_IN_PICTURE_WINDOW_OPEN_DURATION",
      window
    );

    let reason = gCloseReasons.get(window) || "other";
    Services.telemetry.keyedScalarAdd(
      "pictureinpicture.closed_method",
      reason,
      1
    );
    gCurrentPlayerCount -= 1;
    // Saves the location of the Picture in Picture window
    this.savePosition(window);
    this.clearPipTabIcon(window);
  },

  /**
   * Open a Picture in Picture window on the same screen as parentWin,
   * sized based on the information in videoData.
   *
   * @param parentWin (chrome window)
   *   The window hosting the browser that requested the Picture in
   *   Picture window.
   *
   * @param videoData (object)
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   * @param actorReference (PictureInPictureParent)
   * 	Reference to the calling PictureInPictureParent
   *
   * @returns Promise
   *   Resolves once the window has opened and loaded the player component.
   */
  async openPipWindow(parentWin, videoData) {
    let { top, left, width, height } = this.fitToScreen(parentWin, videoData);

    let features =
      `${PLAYER_FEATURES},top=${top},left=${left},` +
      `outerWidth=${width},outerHeight=${height}`;

    let pipWindow = Services.ww.openWindow(
      parentWin,
      PLAYER_URI,
      null,
      features,
      null
    );

    TelemetryStopwatch.start(
      "FX_PICTURE_IN_PICTURE_WINDOW_OPEN_DURATION",
      pipWindow,
      {
        inSeconds: true,
      }
    );

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
   * @param requestingWin (chrome window|player window)
   *   The window hosting the browser that requested the Picture in
   *   Picture window. If this is an existing player window then the returned
   *   player size and position will be determined based on the existing
   *   player window's size and position.
   *
   * @param videoData (object)
   *   An object containing the following properties:
   *
   *   videoHeight (int):
   *     The preferred height of the video.
   *
   *   videoWidth (int):
   *     The preferred width of the video.
   *
   * @returns (object)
   *   The size and position for the player window.
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

    let top, left, width, height;
    if (isPlayer) {
      // requestingWin is a PiP player, conserve its dimensions in this case
      left = requestingWin.screenX;
      top = requestingWin.screenY;
      width = requestingWin.innerWidth;
      height = requestingWin.innerHeight;
    } else {
      // requestingWin is a content window, load last PiP's dimensions
      ({ top, left, width, height } = this.loadPosition());
    }

    // Check that previous location and size were loaded
    if (!isNaN(top) && !isNaN(left) && !isNaN(width) && !isNaN(height)) {
      // Center position of PiP window
      let centerX = left + width / 2;
      let centerY = top + height / 2;

      // Get the screen of the last PiP using the center of the PiP
      // window to check.
      // PiP screen will be the default screen if the center was
      // not on a screen.
      let PiPScreen = this.getWorkingScreen(centerX, centerY);

      // We have the screen, now we will get the dimensions of the screen
      let [
        PiPScreenLeft,
        PiPScreenTop,
        PiPScreenWidth,
        PiPScreenHeight,
      ] = this.getAvailScreenSize(PiPScreen);

      // Check that the center of the last PiP location is within the screen limits
      // If it's not, then we will use the default size and position
      if (
        PiPScreenLeft <= centerX &&
        centerX <= PiPScreenLeft + PiPScreenWidth &&
        PiPScreenTop <= centerY &&
        centerY <= PiPScreenTop + PiPScreenHeight
      ) {
        let oldWidth = width;

        // The new PiP window will keep the height of the old
        // PiP window and adjust the width to the correct ratio
        width = Math.round((height * videoWidth) / videoHeight);

        // Minimum window size on Windows is 136
        if (AppConstants.platform == "win") {
          width = 136 > width ? 136 : width;
        }

        // WIGGLE_ROOM allows the PiP window to be within 5 pixels of the right
        // side of the screen to stay snapped to the right side
        const WIGGLE_ROOM = 5;
        // If the PiP window was right next to the right side of the screen
        // then move the PiP window to the right the same distance that
        // the width changes from previous width to current width
        let rightScreen = PiPScreenLeft + PiPScreenWidth;
        let distFromRight = rightScreen - (left + width);
        if (
          0 < distFromRight &&
          distFromRight <= WIGGLE_ROOM + (oldWidth - width)
        ) {
          left += distFromRight;
        }

        // Checks if some of the PiP window is off screen and
        // if so it will adjust to move everything on screen
        if (left < PiPScreenLeft) {
          // off the left of the screen
          // slide right
          left += PiPScreenLeft - left;
        }
        if (top < PiPScreenTop) {
          // off the top of the screen
          // slide down
          top += PiPScreenTop - top;
        }
        if (left + width > PiPScreenLeft + PiPScreenWidth) {
          // off the right of the screen
          // slide left
          left += PiPScreenLeft + PiPScreenWidth - left - width;
        }
        if (top + height > PiPScreenTop + PiPScreenHeight) {
          // off the bottom of the screen
          // slide up
          top += PiPScreenTop + PiPScreenHeight - top - height;
        }
        return { top, left, width, height };
      }
    }

    // We don't have the size or position of the last PiP window, so fall
    // back to calculating the default location.
    let screen = this.getWorkingScreen(
      requestingWin.screenX,
      requestingWin.screenY,
      requestingWin.innerWidth,
      requestingWin.innerHeight
    );
    let [
      screenLeft,
      screenTop,
      screenWidth,
      screenHeight,
    ] = this.getAvailScreenSize(screen);

    // The Picture in Picture window will be a maximum of a quarter of
    // the screen height, and a third of the screen width.
    const MAX_HEIGHT = screenHeight / 4;
    const MAX_WIDTH = screenWidth / 3;

    width = videoWidth;
    height = videoHeight;
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

    return { top, left, width, height };
  },

  resizePictureInPictureWindow(videoData, actorRef) {
    let win = this.getWeakPipPlayer(actorRef);

    if (!win) {
      return;
    }

    let { top, left, width, height } = this.fitToScreen(win, videoData);
    win.resizeTo(width, height);
    win.moveTo(left, top);
  },

  openToggleContextMenu(window, data) {
    let document = window.document;
    let popup = document.getElementById("pictureInPictureToggleContextMenu");

    // We synthesize a new MouseEvent to propagate the inputSource to the
    // subsequently triggered popupshowing event.
    let newEvent = document.createEvent("MouseEvent");
    newEvent.initNSMouseEvent(
      "contextmenu",
      true,
      true,
      null,
      0,
      data.screenX,
      data.screenY,
      0,
      0,
      false,
      false,
      false,
      false,
      0,
      null,
      0,
      data.mozInputSource
    );
    popup.openPopupAtScreen(newEvent.screenX, newEvent.screenY, true, newEvent);
  },

  hideToggle() {
    Services.prefs.setBoolPref(TOGGLE_ENABLED_PREF, false);
  },

  /**
   * This function takes a screen and will return the left, top, width and
   * height of the screen
   * @param screen
   * The screen we need to get the sizec and coordinates of
   *
   * @returns array
   * Size and location of screen
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
    let fullLeft = {},
      fullTop = {},
      fullWidth = {},
      fullHeight = {};
    screen.GetRectDisplayPix(fullLeft, fullTop, fullWidth, fullHeight);

    // We have to divide these dimensions by the CSS scale factor for the
    // display in order for the video to be positioned correctly on displays
    // that are not at a 1.0 scaling.
    let scaleFactor = screen.contentsScaleFactor / screen.defaultCSSScaleFactor;
    screenWidth.value *= scaleFactor;
    screenHeight.value *= scaleFactor;
    screenLeft.value =
      (screenLeft.value - fullLeft.value) * scaleFactor + fullLeft.value;
    screenTop.value =
      (screenTop.value - fullTop.value) * scaleFactor + fullTop.value;

    return [
      screenLeft.value,
      screenTop.value,
      screenWidth.value,
      screenHeight.value,
    ];
  },

  /**
   * This function takes in a left and top value and returns the screen they
   * are located on.
   * If the left and top are not on any screen, it will return the
   * default screen
   * @param left
   *  left or x coordinate
   * @param top
   *  top or y coordinate
   *
   * @returns screen
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
    let screen = screenManager.screenForRect(left, top, width, height);

    return screen;
  },

  /**
   * Saves position and size of Picture-in-Picture window
   * @param win The Picture-in-Picture window
   */
  savePosition(win) {
    let xulStore = Services.xulStore;

    let left = win.screenX;
    let top = win.screenY;
    let width = win.innerWidth;
    let height = win.innerHeight;

    xulStore.setValue(PLAYER_URI, "picture-in-picture", "left", left);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "top", top);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "width", width);
    xulStore.setValue(PLAYER_URI, "picture-in-picture", "height", height);
  },

  /**
   * Load last Picture in Picture location and size
   * @returns object
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
};

XPCOMUtils.defineLazyPreferenceGetter(
  PictureInPicture,
  "isMultiPipEnabled",
  MULTI_PIP_ENABLED_PREF,
  false
);
