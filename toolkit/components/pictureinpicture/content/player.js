/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PictureInPicture } = ChromeUtils.import(
  "resource://gre/modules/PictureInPicture.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const AUDIO_TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.audio-toggle.enabled";
const KEYBOARD_CONTROLS_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.keyboard-controls.enabled";
const CAPTIONS_TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.display-text-tracks.toggle.enabled";
const TEXT_TRACK_FONT_SIZE_PREF =
  "media.videocontrols.picture-in-picture.display-text-tracks.size";

// Time to fade the Picture-in-Picture video controls after first opening.
const CONTROLS_FADE_TIMEOUT_MS = 3000;
const RESIZE_DEBOUNCE_RATE_MS = 500;

/**
Quadrants!
* 2 | 1
* 3 | 4
*/
const TOP_RIGHT_QUADRANT = 1;
const TOP_LEFT_QUADRANT = 2;
const BOTTOM_LEFT_QUADRANT = 3;
const BOTTOM_RIGHT_QUADRANT = 4;

/**
 * Public function to be called from PictureInPicture.jsm. This is the main
 * entrypoint for initializing the player window.
 *
 * @param {Number} id
 *   A unique numeric ID for the window, used for Telemetry Events.
 * @param {WindowGlobalParent} wgp
 *   The WindowGlobalParent that is hosting the originating video.
 * @param {ContentDOMReference} videoRef
 *    A reference to the video element that a Picture-in-Picture window
 *    is being created for
 */
function setupPlayer(id, wgp, videoRef) {
  Player.init(id, wgp, videoRef);
}

/**
 * Public function to be called from PictureInPicture.jsm. This update the
 * controls based on whether or not the video is playing.
 *
 * @param {Boolean} isPlaying
 *   True if the Picture-in-Picture video is playing.
 */
function setIsPlayingState(isPlaying) {
  Player.isPlaying = isPlaying;
}

/**
 * Public function to be called from PictureInPicture.jsm. This update the
 * controls based on whether or not the video is muted.
 *
 * @param {Boolean} isMuted
 *   True if the Picture-in-Picture video is muted.
 */
function setIsMutedState(isMuted) {
  Player.isMuted = isMuted;
}

function showSubtitlesButton() {
  Player.showSubtitlesButton();
}

/**
 * The Player object handles initializing the player, holds state, and handles
 * events for updating state.
 */
let Player = {
  WINDOW_EVENTS: [
    "click",
    "contextmenu",
    "dblclick",
    "keydown",
    "mouseup",
    "mousemove",
    "MozDOMFullscreen:Entered",
    "MozDOMFullscreen:Exited",
    "resize",
    "unload",
  ],
  actor: null,
  /**
   * Used for resizing Telemetry to avoid recording an event for every resize
   * event. Instead, we wait until RESIZE_DEBOUNCE_RATE_MS has passed since the
   * last resize event before recording.
   */
  resizeDebouncer: null,
  /**
   * Used for window movement Telemetry to determine if the player window has
   * moved since the last time we checked.
   */
  lastScreenX: -1,
  lastScreenY: -1,
  id: -1,

  /**
   * When set to a non-null value, a timer is scheduled to hide the controls
   * after CONTROLS_FADE_TIMEOUT_MS milliseconds.
   */
  showingTimeout: null,

  /**
   * Used to determine old window location when mouseup-ed for corner
   * snapping drag vector calculation
   */
  oldMouseUpWindowX: window.screenX,
  oldMouseUpWindowY: window.screenY,

  /**
   * Used to determine if hovering the mouse cursor over the pip window or not.
   * Gets updated whenever a new hover state is detected.
   */
  isCurrentHover: false,

  /**
   * Initializes the player browser, and sets up the initial state.
   *
   * @param {Number} id
   *   A unique numeric ID for the window, used for Telemetry Events.
   * @param {WindowGlobalParent} wgp
   *   The WindowGlobalParent that is hosting the originating video.
   * @param {ContentDOMReference} videoRef
   *    A reference to the video element that a Picture-in-Picture window
   *    is being created for
   */
  init(id, wgp, videoRef) {
    this.id = id;

    let holder = document.querySelector(".player-holder");
    let browser = document.getElementById("browser");
    browser.remove();

    browser.setAttribute("nodefaultsrc", "true");

    let playPauseBtn = document.getElementById("playpause");
    playPauseBtn.focus({ preventFocusRing: true });

    // Set the specific remoteType and browsingContextGroupID to use for the
    // initial about:blank load. The combination of these two properties will
    // ensure that the browser loads in the same process as our originating
    // browser.
    browser.setAttribute("remoteType", wgp.domProcess.remoteType);
    browser.setAttribute(
      "initialBrowsingContextGroupId",
      wgp.browsingContext.group.id
    );
    holder.appendChild(browser);

    this.actor = browser.browsingContext.currentWindowGlobal.getActor(
      "PictureInPicture"
    );
    this.actor.sendAsyncMessage("PictureInPicture:SetupPlayer", {
      videoRef,
    });

    PictureInPicture.weakPipToWin.set(this.actor, window);

    for (let eventType of this.WINDOW_EVENTS) {
      addEventListener(eventType, this);
    }

    this.controls.addEventListener("mouseleave", () => {
      this.onMouseLeave();
    });
    this.controls.addEventListener("mouseenter", () => {
      this.onMouseEnter();
    });

    for (let radio of document.querySelectorAll(
      'input[type=radio][name="cc-size"]'
    )) {
      radio.addEventListener("change", event => {
        this.onSubtitleChange(event.target.id);
      });
    }

    document
      .querySelector("#subtitles-toggle")
      .addEventListener("change", () => {
        this.onToggleChange();
      });

    // If the content process hosting the video crashes, let's
    // just close the window for now.
    browser.addEventListener("oop-browser-crashed", this);

    this.revealControls(false);

    if (Services.prefs.getBoolPref(AUDIO_TOGGLE_ENABLED_PREF, false)) {
      const audioButton = document.getElementById("audio");
      audioButton.hidden = false;
      audioButton.previousElementSibling.hidden = false;
    }

    this.resizeDebouncer = new DeferredTask(() => {
      this.recordEvent("resize", {
        width: window.outerWidth.toString(),
        height: window.outerHeight.toString(),
      });
    }, RESIZE_DEBOUNCE_RATE_MS);

    this.lastScreenX = window.screenX;
    this.lastScreenY = window.screenY;

    this.computeAndSetMinimumSize(window.outerWidth, window.outerHeight);

    // alwaysontop windows are not focused by default, so we have to do it
    // ourselves. We use requestAnimationFrame since we have to wait until the
    // window is visible before it can focus.
    window.requestAnimationFrame(() => {
      window.focus();
    });

    let fontSize = Services.prefs.getCharPref(
      TEXT_TRACK_FONT_SIZE_PREF,
      "medium"
    );

    // fallback to medium if the pref value is not a valid option
    if (fontSize === "small" || fontSize === "large") {
      document.querySelector(`#${fontSize}`).checked = "true";
    } else {
      document.querySelector("#medium").checked = "true";
    }
  },

  uninit() {
    this.resizeDebouncer.disarm();
    PictureInPicture.unload(window, this.actor);
  },

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        // Don't run onClick if middle or right click is pressed respectively
        if (event.button !== 1 && event.button !== 2) {
          this.onClick(event);
          this.controls.removeAttribute("keying");
        }
        break;
      }

      case "contextmenu": {
        event.preventDefault();
        break;
      }

      case "dblclick": {
        this.onDblClick(event);
        break;
      }

      case "keydown": {
        if (event.keyCode == KeyEvent.DOM_VK_TAB) {
          this.controls.setAttribute("keying", true);
          this.actor.sendAsyncMessage("PictureInPicture:ShowVideoControls", {
            isFullscreen: this.isFullscreen,
            isVideoControlsShowing: true,
            playerBottomControlsDOMRect: this.controlsBottom.getBoundingClientRect(),
          });
        } else if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
          event.preventDefault();
          if (this.isFullscreen) {
            // We handle the ESC key, in fullscreen modus as intent to leave only the fullscreen mode
            document.exitFullscreen();
          } else {
            // We handle the ESC key, as an intent to leave the picture-in-picture modus
            this.onClose();
          }
        } else if (
          Services.prefs.getBoolPref(KEYBOARD_CONTROLS_ENABLED_PREF, false) &&
          (event.keyCode != KeyEvent.DOM_VK_SPACE || !event.target.id)
        ) {
          // Pressing "space" fires a "keydown" event which can also trigger a control
          // button's "click" event. Handle the "keydown" event only when the event did
          // not originate from a control button and it is not a "space" keypress.
          this.onKeyDown(event);
        }

        break;
      }

      case "mouseup": {
        this.onMouseUp(event);
        break;
      }

      case "mousemove": {
        this.onMouseMove();
        break;
      }

      // Normally, the DOMFullscreenParent / DOMFullscreenChild actors
      // would take care of firing the `fullscreen-painted` notification,
      // however, those actors are only ever instantiated when a <browser>
      // is fullscreened, and not a <body> element in a parent-process
      // chrome privileged DOM window.
      //
      // Rather than trying to re-engineer JSWindowActors to be re-usable for
      // this edge-case, we do the work of firing fullscreen-painted when
      // transitioning in and out of fullscreen ourselves here.
      case "MozDOMFullscreen:Entered":
      // Intentional fall-through
      case "MozDOMFullscreen:Exited": {
        let { lastTransactionId } = window.windowUtils;
        window.addEventListener("MozAfterPaint", function onPainted(event) {
          if (event.transactionId > lastTransactionId) {
            window.removeEventListener("MozAfterPaint", onPainted);
            Services.obs.notifyObservers(window, "fullscreen-painted");
          }
        });
        if (this.isFullscreen) {
          window.focus();
          this.actor.sendAsyncMessage("PictureInPicture:EnterFullscreen", {
            isFullscreen: true,
            isVideoControlsShowing: null,
            playerBottomControlsDOMRect: null,
          });
        } else {
          this.actor.sendAsyncMessage("PictureInPicture:ExitFullscreen", {
            isFullscreen: this.isFullscreen,
            isVideoControlsShowing:
              !!this.controls.getAttribute("showing") ||
              !!this.controls.getAttribute("keying"),
            playerBottomControlsDOMRect: this.controlsBottom.getBoundingClientRect(),
          });
        }
        break;
      }

      case "oop-browser-crashed": {
        this.closePipWindow({ reason: "browser-crash" });
        break;
      }

      case "resize": {
        this.onResize(event);
        break;
      }

      case "unload": {
        this.uninit();
        break;
      }
    }
  },

  closePipWindow(closeData) {
    // Set the subtitles font size prefs
    Services.prefs.setBoolPref(
      CAPTIONS_TOGGLE_ENABLED_PREF,
      document.querySelector("#subtitles-toggle").checked
    );
    for (let radio of document.querySelectorAll(
      'input[type=radio][name="cc-size"]'
    )) {
      if (radio.checked) {
        Services.prefs.setCharPref(TEXT_TRACK_FONT_SIZE_PREF, radio.id);
        break;
      }
    }
    const { reason } = closeData;
    PictureInPicture.closeSinglePipWindow({ reason, actorRef: this.actor });
  },

  onDblClick(event) {
    if (event.target.id == "controls") {
      if (this.isFullscreen) {
        document.exitFullscreen();
      } else {
        document.body.requestFullscreen();
      }
      event.preventDefault();
    }
  },

  onClick(event) {
    switch (event.target.id) {
      case "audio": {
        if (this.isMuted) {
          this.actor.sendAsyncMessage("PictureInPicture:Unmute");
        } else {
          this.actor.sendAsyncMessage("PictureInPicture:Mute");
        }
        break;
      }

      case "close": {
        this.onClose();
        break;
      }

      case "playpause": {
        if (!this.isPlaying) {
          this.actor.sendAsyncMessage("PictureInPicture:Play");
          this.revealControls(false);
        } else {
          this.actor.sendAsyncMessage("PictureInPicture:Pause");
          this.revealControls(true);
        }

        break;
      }

      case "unpip": {
        PictureInPicture.focusTabAndClosePip(window, this.actor);
        break;
      }

      case "closed-caption": {
        let settingsPanel = document.querySelector("#settings");
        let settingsPanelVisible = !settingsPanel.classList.contains("hide");
        if (settingsPanelVisible) {
          settingsPanel.classList.add("hide");
          this.controls.removeAttribute("donthide");
        } else {
          settingsPanel.classList.remove("hide");
          this.controls.setAttribute("donthide", true);
        }
        break;
      }
    }
  },

  onClose() {
    this.actor.sendAsyncMessage("PictureInPicture:Pause", {
      reason: "pip-closed",
    });
    this.closePipWindow({ reason: "close-button" });
  },

  onKeyDown(event) {
    // We don't want to send a keydown event if the event target was one of the
    // font sizes in the settings panel
    if (
      event.target.parentElement?.parentElement?.classList?.contains(
        "font-size-selection"
      )
    ) {
      return;
    }
    this.actor.sendAsyncMessage("PictureInPicture:KeyDown", {
      altKey: event.altKey,
      shiftKey: event.shiftKey,
      metaKey: event.metaKey,
      ctrlKey: event.ctrlKey,
      keyCode: event.keyCode,
    });
  },

  onSubtitleChange(size) {
    Services.prefs.setCharPref(TEXT_TRACK_FONT_SIZE_PREF, size);

    this.actor.sendAsyncMessage("PictureInPicture:ChangeFontSizeTextTracks");
  },

  onToggleChange() {
    // The subtitles toggle has been click in the settings panel so we toggle
    // the overlay above the font sizes and send a message to toggle the
    // visibility of the subtitles and set the toggle pref
    document
      .querySelector(".font-size-selection")
      .classList.toggle("font-size-overlay");
    this.actor.sendAsyncMessage("PictureInPicture:ToggleTextTracks");

    this.captionsToggleEnabled = !this.captionsToggleEnabled;
    Services.prefs.setBoolPref(
      CAPTIONS_TOGGLE_ENABLED_PREF,
      this.captionsToggleEnabled
    );
  },

  /**
   * PiP Corner Snapping Helper Function
   * Determines the quadrant the PiP window is currently in.
   */
  determineCurrentQuadrant() {
    // Determine center coordinates of window.
    let windowCenterX = window.screenX + window.innerWidth / 2;
    let windowCenterY = window.screenY + window.innerHeight / 2;
    let quadrant = null;
    let halfWidth = window.screen.availLeft + window.screen.availWidth / 2;
    let halfHeight = window.screen.availTop + window.screen.availHeight / 2;

    let leftHalf = windowCenterX < halfWidth;
    let rightHalf = windowCenterX > halfWidth;
    let topHalf = windowCenterY < halfHeight;
    let bottomHalf = windowCenterY > halfHeight;

    if (leftHalf && topHalf) {
      quadrant = TOP_LEFT_QUADRANT;
    } else if (rightHalf && topHalf) {
      quadrant = TOP_RIGHT_QUADRANT;
    } else if (leftHalf && bottomHalf) {
      quadrant = BOTTOM_LEFT_QUADRANT;
    } else if (rightHalf && bottomHalf) {
      quadrant = BOTTOM_RIGHT_QUADRANT;
    }
    return quadrant;
  },

  /**
   * Helper function to actually move/snap the PiP window.
   * Moves the PiP window to the top right.
   */
  moveToTopRight() {
    window.moveTo(
      window.screen.availLeft + window.screen.availWidth - window.innerWidth,
      window.screen.availTop
    );
  },

  /**
   * Moves the PiP window to the top left.
   */
  moveToTopLeft() {
    window.moveTo(window.screen.availLeft, window.screen.availTop);
  },

  /**
   * Moves the PiP window to the bottom right.
   */
  moveToBottomRight() {
    window.moveTo(
      window.screen.availLeft + window.screen.availWidth - window.innerWidth,
      window.screen.availTop + window.screen.availHeight - window.innerHeight
    );
  },

  /**
   * Moves the PiP window to the bottom left.
   */
  moveToBottomLeft() {
    window.moveTo(
      window.screen.availLeft,
      window.screen.availTop + window.screen.availHeight - window.innerHeight
    );
  },

  /**
   * Uses the PiP window's change in position to determine which direction
   * the window has been moved in.
   */
  determineDirectionDragged() {
    // Determine change in window location.
    let deltaX = this.oldMouseUpWindowX - window.screenX;
    let deltaY = this.oldMouseUpWindowY - window.screenY;
    let dragDirection = "";

    if (Math.abs(deltaX) > Math.abs(deltaY) && deltaX < 0) {
      dragDirection = "draggedRight";
    } else if (Math.abs(deltaX) > Math.abs(deltaY) && deltaX > 0) {
      dragDirection = "draggedLeft";
    } else if (Math.abs(deltaX) < Math.abs(deltaY) && deltaY < 0) {
      dragDirection = "draggedDown";
    } else if (Math.abs(deltaX) < Math.abs(deltaY) && deltaY > 0) {
      dragDirection = "draggedUp";
    }
    return dragDirection;
  },

  /**
   * Event handler for "mouseup" events on the PiP window.
   *
   * @param {Event} event
   *  Event context details
   */
  onMouseUp(event) {
    this.lastScreenX = window.screenX;
    this.lastScreenY = window.screenY;

    // Corner snapping changes start here.
    // Check if metakey pressed and macOS
    let quadrant = this.determineCurrentQuadrant();
    let dragAction = this.determineDirectionDragged();

    if (event.metaKey && AppConstants.platform == "macosx" && dragAction) {
      // Moving logic based on current quadrant and direction of drag.
      switch (quadrant) {
        case TOP_RIGHT_QUADRANT:
          switch (dragAction) {
            case "draggedRight":
              this.moveToTopRight();
              break;
            case "draggedLeft":
              this.moveToTopLeft();
              break;
            case "draggedDown":
              this.moveToBottomRight();
              break;
            case "draggedUp":
              this.moveToTopRight();
              break;
          }
          break;
        case TOP_LEFT_QUADRANT:
          switch (dragAction) {
            case "draggedRight":
              this.moveToTopRight();
              break;
            case "draggedLeft":
              this.moveToTopLeft();
              break;
            case "draggedDown":
              this.moveToBottomLeft();
              break;
            case "draggedUp":
              this.moveToTopLeft();
              break;
          }
          break;
        case BOTTOM_LEFT_QUADRANT:
          switch (dragAction) {
            case "draggedRight":
              this.moveToBottomRight();
              break;
            case "draggedLeft":
              this.moveToBottomLeft();
              break;
            case "draggedDown":
              this.moveToBottomLeft();
              break;
            case "draggedUp":
              this.moveToTopLeft();
              break;
          }
          break;
        case BOTTOM_RIGHT_QUADRANT:
          switch (dragAction) {
            case "draggedRight":
              this.moveToBottomRight();
              break;
            case "draggedLeft":
              this.moveToBottomLeft();
              break;
            case "draggedDown":
              this.moveToBottomRight();
              break;
            case "draggedUp":
              this.moveToTopRight();
              break;
          }
          break;
      } // Switch close.
    } // Metakey close.
    this.oldMouseUpWindowX = window.screenX;
    this.oldMouseUpWindowY = window.screenY;
  },

  /**
   * Event handler for mousemove the PiP Window
   */
  onMouseMove() {
    if (this.isFullscreen) {
      this.revealControls(false);
    }
  },

  onMouseEnter() {
    if (!this.isFullscreen) {
      this.isCurrentHover = true;
      this.actor.sendAsyncMessage("PictureInPicture:ShowVideoControls", {
        isFullscreen: this.isFullscreen,
        isVideoControlsShowing: true,
        playerBottomControlsDOMRect: this.controlsBottom.getBoundingClientRect(),
      });
    }
  },

  onMouseLeave() {
    if (!this.isFullscreen && !this.controls.getAttribute("donthide")) {
      this.isCurrentHover = false;
      if (
        !this.controls.getAttribute("showing") &&
        !this.controls.getAttribute("keying")
      ) {
        this.actor.sendAsyncMessage("PictureInPicture:HideVideoControls", {
          isFullscreen: this.isFullscreen,
          isVideoControlsShowing: false,
          playerBottomControlsDOMRect: null,
        });
      }
    }
  },

  showSubtitlesButton() {
    let subtitlesContent = document.querySelectorAll(".subtitles");
    for (let ele of subtitlesContent) {
      ele.hidden = false;
    }
    this.captionsToggleEnabled = true;
    // If the CAPTIONS_TOGGLE_ENABLED_PREF pref is false then we will click
    // the UI toggle to change the toggle to unchecked. This will call
    // onToggleChange where this.captionsToggleEnabled will be updated
    if (!Services.prefs.getBoolPref(CAPTIONS_TOGGLE_ENABLED_PREF, true)) {
      document.querySelector("#subtitles-toggle").click();
    }
  },

  /**
   * Event handler for resizing the PiP Window
   *
   * @param {Event} event
   *  Event context data object
   */
  onResize(event) {
    this.resizeDebouncer.disarm();
    this.resizeDebouncer.arm();
  },

  /**
   * Event handler for user issued commands
   *
   * @param {Event} event
   *  Event context data object
   */
  onCommand(event) {
    this.closePipWindow({ reason: "player-shortcut" });
  },

  get controls() {
    delete this.controls;
    return (this.controls = document.getElementById("controls"));
  },

  get controlsBottom() {
    delete this.controlsBottom;
    return (this.controlsBottom = document.getElementById("controls-bottom"));
  },

  _isPlaying: false,
  /**
   * GET isPlaying returns true if the video is currently playing.
   *
   * SET isPlaying to true if the video is playing, false otherwise. This will
   * update the internal state and displayed controls.
   *
   * @type {Boolean}
   */
  get isPlaying() {
    return this._isPlaying;
  },

  set isPlaying(isPlaying) {
    this._isPlaying = isPlaying;
    this.controls.classList.toggle("playing", isPlaying);
    const playButton = document.getElementById("playpause");
    let strId = "pictureinpicture-" + (isPlaying ? "pause" : "play");
    document.l10n.setAttributes(playButton, strId);
  },

  _isMuted: false,
  /**
   * GET isMuted returns true if the video is currently muted.
   *
   * SET isMuted to true if the video is muted, false otherwise. This will
   * update the internal state and displayed controls.
   *
   * @type {Boolean}
   */
  get isMuted() {
    return this._isMuted;
  },

  set isMuted(isMuted) {
    this._isMuted = isMuted;
    this.controls.classList.toggle("muted", isMuted);
    const audioButton = document.getElementById("audio");
    let strId = "pictureinpicture-" + (isMuted ? "unmute" : "mute");
    document.l10n.setAttributes(audioButton, strId);
  },

  /**
   * GET isFullscreen returns true if the video is running in fullscreen mode
   *
   * @returns {boolean}
   */
  get isFullscreen() {
    return document.fullscreenElement == document.body;
  },

  /**
   * Used for recording telemetry in Picture-in-Picture.
   *
   * @param {string} type
   *   The type of PiP event being recorded.
   * @param {object} args
   *   The data to pass to telemetry when the event is recorded.
   */
  recordEvent(type, args) {
    Services.telemetry.recordEvent(
      "pictureinpicture",
      type,
      "player",
      this.id,
      args
    );
  },

  /**
   * Makes the player controls visible.
   *
   * @param {Boolean} revealIndefinitely
   *   If false, this will hide the controls again after
   *   CONTROLS_FADE_TIMEOUT_MS milliseconds has passed. If true, the controls
   *   will remain visible until revealControls is called again with
   *   revealIndefinitely set to false.
   */
  revealControls(revealIndefinitely) {
    clearTimeout(this.showingTimeout);
    this.showingTimeout = null;

    this.controls.setAttribute("showing", true);

    if (!this.isFullscreen) {
      // revealControls() is called everytime we hover over fullscreen pip window.
      // Only communicate with pipchild when not in fullscreen mode for performance reasons.
      this.actor.sendAsyncMessage("PictureInPicture:ShowVideoControls", {
        isFullscreen: false,
        isVideoControlsShowing: true,
        playerBottomControlsDOMRect: this.controlsBottom.getBoundingClientRect(),
      });
    }

    if (!revealIndefinitely && !this.controls.getAttribute("donthide")) {
      this.showingTimeout = setTimeout(() => {
        this.controls.removeAttribute("showing");

        if (
          !this.isFullscreen &&
          !this.isCurrentHover &&
          !this.controls.getAttribute("keying")
        ) {
          this.actor.sendAsyncMessage("PictureInPicture:HideVideoControls", {
            isFullscreen: false,
            isVideoControlsShowing: false,
            playerBottomControlsDOMRect: null,
          });
        }
      }, CONTROLS_FADE_TIMEOUT_MS);
    }
  },

  /**
   * Given a width and height for a video, computes the minimum dimensions for
   * the player window, and then sets them on the root element.
   *
   * This is currently only used on Linux GTK, where the OS doesn't already
   * impose a minimum window size. For other platforms, this function is a
   * no-op.
   *
   * @param {Number} width
   *   The width of the video being played.
   * @param {Number} height
   *   The height of the video being played.
   */
  computeAndSetMinimumSize(width, height) {
    if (!AppConstants.MOZ_WIDGET_GTK) {
      return;
    }

    // Using inspection, these seem to be the right minimums for each dimension
    // so that the controls don't get too crowded.
    const MIN_WIDTH = 120;
    const MIN_HEIGHT = 80;

    let resultWidth = width;
    let resultHeight = height;
    let aspectRatio = width / height;

    // Take the smaller of the two dimensions, and set it to the minimum.
    // Then calculate the other dimension using the aspect ratio to get
    // both minimums.
    if (width < height) {
      resultWidth = MIN_WIDTH;
      resultHeight = Math.round(MIN_WIDTH / aspectRatio);
    } else {
      resultHeight = MIN_HEIGHT;
      resultWidth = Math.round(MIN_HEIGHT * aspectRatio);
    }

    document.documentElement.style.minWidth = resultWidth + "px";
    document.documentElement.style.minHeight = resultHeight + "px";
  },
};
