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

    // If the content process hosting the video crashes, let's
    // just close the window for now.
    browser.addEventListener("oop-browser-crashed", this);

    this.revealControls(false);

    if (Services.prefs.getBoolPref(AUDIO_TOGGLE_ENABLED_PREF, false)) {
      const audioButton = document.getElementById("audio");
      audioButton.hidden = false;
      audioButton.previousElementSibling.hidden = false;
    }

    Services.telemetry.setEventRecordingEnabled("pictureinpicture", true);

    this.resizeDebouncer = new DeferredTask(() => {
      this.recordEvent("resize", {
        width: window.outerWidth.toString(),
        height: window.outerHeight.toString(),
      });
    }, RESIZE_DEBOUNCE_RATE_MS);

    this.lastScreenX = window.screenX;
    this.lastScreenY = window.screenY;

    this.recordEvent("create", {
      width: window.outerWidth.toString(),
      height: window.outerHeight.toString(),
      screenX: window.screenX.toString(),
      screenY: window.screenY.toString(),
    });

    this.computeAndSetMinimumSize(window.outerWidth, window.outerHeight);

    // alwaysontop windows are not focused by default, so we have to do it
    // ourselves. We use requestAnimationFrame since we have to wait until the
    // window is visible before it can focus.
    window.requestAnimationFrame(() => {
      window.focus();
    });
  },

  uninit() {
    this.resizeDebouncer.disarm();
    PictureInPicture.unload(window, this.actor);
  },

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
        this.controls.removeAttribute("keying");
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
        } else if (
          event.keyCode == KeyEvent.DOM_VK_ESCAPE &&
          this.controls.hasAttribute("keying")
        ) {
          this.controls.removeAttribute("keying");

          // We preventDefault to avoid exiting fullscreen if we happen
          // to be in it.
          event.preventDefault();
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
    const { reason } = closeData;

    if (PictureInPicture.isMultiPipEnabled) {
      PictureInPicture.closeSinglePipWindow({ reason, actorRef: this.actor });
    } else {
      PictureInPicture.closeAllPipWindows({ reason });
    }
  },

  onDblClick(event) {
    if (event.target.id == "controls") {
      if (document.fullscreenElement == document.body) {
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
        this.actor.sendAsyncMessage("PictureInPicture:Pause", {
          reason: "pip-closed",
        });
        this.closePipWindow({ reason: "close-button" });
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
    }
  },

  onKeyDown(event) {
    this.actor.sendAsyncMessage("PictureInPicture:KeyDown", {
      altKey: event.altKey,
      shiftKey: event.shiftKey,
      metaKey: event.metaKey,
      ctrlKey: event.ctrlKey,
      keyCode: event.keyCode,
    });
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
    if (
      window.screenX != this.lastScreenX ||
      window.screenY != this.lastScreenY
    ) {
      this.recordEvent("move", {
        screenX: window.screenX.toString(),
        screenY: window.screenY.toString(),
      });
    }
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
    if (document.fullscreenElement) {
      this.revealControls(false);
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
    if (!revealIndefinitely) {
      this.showingTimeout = setTimeout(() => {
        this.controls.removeAttribute("showing");
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
