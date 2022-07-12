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

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
XPCOMUtils.defineLazyServiceGetters(lazy, {
  WindowsUIUtils: ["@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils"],
});

const { Rect, Point } = ChromeUtils.import(
  "resource://gre/modules/Geometry.jsm"
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
const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const TOGGLE_POSITION_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.position";
const TOGGLE_POSITION_RIGHT = "right";
const TOGGLE_POSITION_LEFT = "left";
const RESIZE_MARGIN_PX = 16;

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
        PictureInPicture.closeSinglePipWindow({ reason, actorRef: this });
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
      case "PictureInPicture:ShowSubtitlesButton": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.showSubtitlesButton();
        }
        break;
      }
      case "PictureInPicture:HideSubtitlesButton": {
        let player = PictureInPicture.getWeakPipPlayer(this);
        if (player) {
          player.hideSubtitlesButton();
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

  // Maps a browser to the number of PiP windows it has
  browserWeakMap: new WeakMap(),

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

  handleEvent(event) {
    switch (event.type) {
      case "TabSwapPictureInPicture": {
        this.onPipSwappedBrowsers(event);
      }
    }
  },

  /**
   * Increase the count of PiP windows for a given browser
   * @param browser The browser to increase PiP count in browserWeakMap
   */
  addPiPBrowserToWeakMap(browser) {
    let count = this.browserWeakMap.has(browser)
      ? this.browserWeakMap.get(browser)
      : 0;
    this.browserWeakMap.set(browser, count + 1);
  },

  /**
   * Decrease the count of PiP windows for a given browser.
   * If the count becomes 0, we will remove the browser from the WeakMap
   * @param browser The browser to decrease PiP count in browserWeakMap
   */
  removePiPBrowserFromWeakMap(browser) {
    let count = this.browserWeakMap.get(browser);
    if (count <= 1) {
      this.browserWeakMap.delete(browser);
    } else {
      this.browserWeakMap.set(browser, count - 1);
    }
  },

  onPipSwappedBrowsers(event) {
    let otherTab = event.detail;
    if (otherTab) {
      for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
        if (this.weakWinToBrowser.get(win) === event.target.linkedBrowser) {
          this.weakWinToBrowser.set(win, otherTab.linkedBrowser);
          this.removePiPBrowserFromWeakMap(event.target.linkedBrowser);
          this.addPiPBrowserToWeakMap(otherTab.linkedBrowser);
        }
      }
      otherTab.addEventListener("TabSwapPictureInPicture", this);
    }
  },

  /**
   * Called when the browser UI handles the View:PictureInPicture command via
   * the keyboard.
   *
   * @param {Event} event
   */
  onCommand(event) {
    if (!Services.prefs.getBoolPref(PIP_ENABLED_PREF, false)) {
      return;
    }

    let win = event.target.ownerGlobal;
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

    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);

    // focus the tab's window
    tab.ownerGlobal.focus();

    gBrowser.selectedTab = tab;
    await this.closeSinglePipWindow({ reason: "unpip", actorRef: pipActor });
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

    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
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
    this.removePiPBrowserFromWeakMap(this.weakWinToBrowser.get(win));

    let args = { reason };
    Services.telemetry.recordEvent(
      "pictureinpicture",
      "closed_method",
      "method",
      null,
      args
    );
    await this.closePipWindow(win);
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
   * @returns {Promise}
   *   Resolves once the Picture in Picture window has been created, and
   *   the player component inside it has finished loading.
   */
  async handlePictureInPictureRequest(wgp, videoData) {
    gCurrentPlayerCount += 1;

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

    let pipId = gNextWindowID.toString();
    win.setupPlayer(pipId, wgp, videoData.videoRef);
    gNextWindowID++;

    this.weakWinToBrowser.set(win, browser);
    this.addPiPBrowserToWeakMap(browser);

    Services.prefs.setBoolPref(
      "media.videocontrols.picture-in-picture.video-toggle.has-used",
      true
    );

    let args = {
      width: win.innerWidth.toString(),
      height: win.innerHeight.toString(),
      screenX: win.screenX.toString(),
      screenY: win.screenY.toString(),
      ccEnabled: videoData.ccEnabled.toString(),
      webVTTSubtitles: videoData.webVTTSubtitles.toString(),
    };

    Services.telemetry.recordEvent(
      "pictureinpicture",
      "create",
      "player",
      pipId,
      args
    );
  },

  /**
   * unload event has been called in player.js, cleanup our preserved
   * browser object.
   *
   * @param {Window} window
   */
  unload(window) {
    TelemetryStopwatch.finish(
      "FX_PICTURE_IN_PICTURE_WINDOW_OPEN_DURATION",
      window
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

    pipWindow.windowUtils.setResizeMargin(RESIZE_MARGIN_PX);

    if (Services.appinfo.OS == "WINNT") {
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
    if (isPlayer) {
      // requestingWin is a PiP player, conserve its dimensions in this case
      left = requestingWin.screenX * requestingCssToDesktopScale;
      top = requestingWin.screenY * requestingCssToDesktopScale;
      width = requestingWin.outerWidth;
      height = requestingWin.outerHeight;
    } else {
      // requestingWin is a content window, load last PiP's dimensions
      ({ top, left, width, height } = this.loadPosition());
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
    let [
      screenLeft,
      screenTop,
      screenWidth,
      screenHeight,
    ] = this.getAvailScreenSize(screen);

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

    const [
      screenTop,
      screenLeft,
      screenWidth,
      screenHeight,
    ] = this.getAvailScreenSize(conflictScreen);

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

    let { top, left, width, height } = this.fitToScreen(win, videoData);
    win.resizeTo(width, height);
    win.moveTo(left, top);
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
    let newEvent = document.createEvent("MouseEvent");
    let screenX = data.screenXDevPx / window.devicePixelRatio;
    let screenY = data.screenYDevPx / window.devicePixelRatio;
    newEvent.initNSMouseEvent(
      "contextmenu",
      true,
      true,
      null,
      0,
      screenX,
      screenY,
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
   * This is used in AsyncTabSwitcher.jsm and tabbrowser.js to check if the browser
   * currently has a PiP window.
   * If the browser has a PiP window we want to keep the browser in an active state because
   * the browser is still partially visible.
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
   * @param {Screen} screen
   * The screen we need to get the size and coordinates of
   *
   * @returns {array}
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
};
