/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PictureInPicture"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PLAYER_URI = "chrome://global/content/pictureinpicture/player.xhtml";
const PLAYER_FEATURES = `chrome,titlebar=no,alwaysontop,lockaspectratio,resizable`;
const WINDOW_TYPE = "Toolkit:PictureInPicture";
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
 * To differentiate windows in the Telemetry Event Log, each Picture-in-Picture
 * player window is given a unique ID.
 */
let gNextWindowID = 0;

/**
 * This module is responsible for creating a Picture in Picture window to host
 * a clone of a video element running in web content.
 */

var PictureInPicture = {
  // Listeners are added in nsBrowserGlue.js lazily
  receiveMessage(aMessage) {
    let browser = aMessage.target;

    switch (aMessage.name) {
      case "PictureInPicture:Request": {
        let videoData = aMessage.data;
        this.handlePictureInPictureRequest(browser, videoData);
        break;
      }
      case "PictureInPicture:Close": {
        /**
         * Content has requested that its Picture in Picture window go away.
         */
        let reason = aMessage.data.reason;
        this.closePipWindow({ reason });
        break;
      }
      case "PictureInPicture:Playing": {
        let player = this.weakPipPlayer && this.weakPipPlayer.get();
        if (player) {
          player.setIsPlayingState(true);
        }
        break;
      }
      case "PictureInPicture:Paused": {
        let player = this.weakPipPlayer && this.weakPipPlayer.get();
        if (player) {
          player.setIsPlayingState(false);
        }
        break;
      }
      case "PictureInPicture:OpenToggleContextMenu": {
        let win = browser.ownerGlobal;
        this.openToggleContextMenu(win, aMessage.data);
        break;
      }
    }
  },

  async focusTabAndClosePip() {
    let gBrowser = this.browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(this.browser);
    gBrowser.selectedTab = tab;
    await this.closePipWindow({ reason: "unpip" });
  },

  /**
   * Remove attribute which enables pip icon in tab
   */
  clearPipTabIcon() {
    let win = this.browser.ownerGlobal;
    let tab = win.gBrowser.getTabForBrowser(this.browser);
    if (tab) {
      tab.removeAttribute("pictureinpicture");
    }
  },

  /**
   * Find and close any pre-existing Picture in Picture windows.
   */
  async closePipWindow({ reason }) {
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
   * @param browser (xul:browser)
   *   The browser that is requesting the Picture in Picture window.
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
  async handlePictureInPictureRequest(browser, videoData) {
    // If there's a pre-existing PiP window, close it first.
    await this.closePipWindow({ reason: "new-pip" });

    let parentWin = browser.ownerGlobal;
    this.browser = browser;
    let win = await this.openPipWindow(parentWin, videoData);
    this.weakPipPlayer = Cu.getWeakReference(win);
    win.setIsPlayingState(videoData.playing);

    // set attribute which shows pip icon in tab
    let tab = parentWin.gBrowser.getTabForBrowser(browser);
    tab.setAttribute("pictureinpicture", true);

    win.setupPlayer(gNextWindowID.toString(), browser);
    gNextWindowID++;
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

    this.clearPipTabIcon();
    delete this.weakPipPlayer;
    delete this.browser;
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
   * @returns Promise
   *   Resolves once the window has opened and loaded the player component.
   */
  async openPipWindow(parentWin, videoData) {
    let { videoHeight, videoWidth } = videoData;

    // The Picture in Picture window will open on the same display as the
    // originating window, and anchor to the bottom right.
    let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
      Ci.nsIScreenManager
    );
    let screen = screenManager.screenForRect(
      parentWin.screenX,
      parentWin.screenY,
      1,
      1
    );

    // Now that we have the right screen, let's see how much available
    // real-estate there is for us to work with.
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

    // We have to divide these dimensions by the CSS scale factor for the
    // display in order for the video to be positioned correctly on displays
    // that are not at a 1.0 scaling.
    screenWidth.value = screenWidth.value / screen.defaultCSSScaleFactor;
    screenHeight.value = screenHeight.value / screen.defaultCSSScaleFactor;

    // For now, the Picture in Picture window will be a maximum of a quarter
    // of the screen height, and a third of the screen width.
    const MAX_HEIGHT = screenHeight.value / 4;
    const MAX_WIDTH = screenWidth.value / 3;

    let resultWidth = videoWidth;
    let resultHeight = videoHeight;

    if (videoHeight > MAX_HEIGHT || videoWidth > MAX_WIDTH) {
      let aspectRatio = videoWidth / videoHeight;
      // We're bigger than the max - take the largest dimension and clamp
      // it to the associated max. Recalculate the other dimension to maintain
      // aspect ratio.
      if (videoWidth >= videoHeight) {
        // We're clamping the width, so the height must be adjusted to match
        // the original aspect ratio. Since aspect ratio is width over height,
        // that means we need to _divide_ the MAX_WIDTH by the aspect ratio to
        // calculate the appropriate height.
        resultWidth = MAX_WIDTH;
        resultHeight = Math.round(MAX_WIDTH / aspectRatio);
      } else {
        // We're clamping the height, so the width must be adjusted to match
        // the original aspect ratio. Since aspect ratio is width over height,
        // this means we need to _multiply_ the MAX_HEIGHT by the aspect ratio
        // to calculate the appropriate width.
        resultHeight = MAX_HEIGHT;
        resultWidth = Math.round(MAX_HEIGHT * aspectRatio);
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
    let pipLeft = isRTL
      ? screenLeft.value
      : screenLeft.value + screenWidth.value - resultWidth;
    let pipTop = screenTop.value + screenHeight.value - resultHeight;
    let features =
      `${PLAYER_FEATURES},top=${pipTop},left=${pipLeft},` +
      `outerWidth=${resultWidth},outerHeight=${resultHeight}`;

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
};
