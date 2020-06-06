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
 * Public function to be called from PictureInPicture.jsm. This is the main
 * entrypoint for initializing the player window.
 *
 * @param id (Number)
 *   A unique numeric ID for the window, used for Telemetry Events.
 * @param originatingBrowser (xul:browser)
 *   The <xul:browser> that the Picture-in-Picture video is coming from.
 */
function setupPlayer(id, originatingBrowser) {
  Player.init(id, originatingBrowser);
}

/**
 * Public function to be called from PictureInPicture.jsm. This update the
 * controls based on whether or not the video is playing.
 *
 * @param isPlaying (Boolean)
 *   True if the Picture-in-Picture video is playing.
 */
function setIsPlayingState(isPlaying) {
  Player.isPlaying = isPlaying;
}

/**
 * Public function to be called from PictureInPicture.jsm. This update the
 * controls based on whether or not the video is muted.
 *
 * @param isMuted (Boolean)
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
    "mouseout",
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
   * Initializes the player browser, and sets up the initial state.
   *
   * @param id (Number)
   *   A unique numeric ID for the window, used for Telemetry Events.
   * @param originatingBrowser (xul:browser)
   *   The <xul:browser> that the Picture-in-Picture video is coming from.
   */
  init(id, originatingBrowser) {
    this.id = id;

    let holder = document.querySelector(".player-holder");
    let browser = document.getElementById("browser");
    browser.remove();

    browser.setAttribute("nodefaultsrc", "true");
    browser.sameProcessAsFrameLoader = originatingBrowser.frameLoader;
    holder.appendChild(browser);

    this.actor = browser.browsingContext.currentWindowGlobal.getActor(
      "PictureInPicture"
    );
    this.actor.sendAsyncMessage("PictureInPicture:SetupPlayer");

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
  },

  uninit() {
    this.resizeDebouncer.disarm();
    PictureInPicture.unload(window);
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
          !this.controls.hasAttribute("keying") &&
          (event.keyCode != KeyEvent.DOM_VK_SPACE || !event.target.id)
        ) {
          // Pressing "space" fires a "keydown" event which can also trigger a control
          // button's "click" event. Handle the "keydown" event only when the event did
          // not originate from a control button and it is not a "space" keypress.
          this.onKeyDown(event);
        }

        break;
      }

      case "mouseout": {
        this.onMouseOut(event);
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
        PictureInPicture.closePipWindow({ reason: "browser-crash" });
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
        this.actor.sendAsyncMessage("PictureInPicture:Pause");
        PictureInPicture.closePipWindow({ reason: "close-button" });
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
        PictureInPicture.focusTabAndClosePip();
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

  onMouseOut(event) {
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
  },

  onResize(event) {
    this.resizeDebouncer.disarm();
    this.resizeDebouncer.arm();
  },

  get controls() {
    delete this.controls;
    return (this.controls = document.getElementById("controls"));
  },

  _isPlaying: false,
  /**
   * isPlaying returns true if the video is currently playing.
   *
   * @return Boolean
   */
  get isPlaying() {
    return this._isPlaying;
  },

  /**
   * Set isPlaying to true if the video is playing, false otherwise. This will
   * update the internal state and displayed controls.
   */
  set isPlaying(isPlaying) {
    this._isPlaying = isPlaying;
    this.controls.classList.toggle("playing", isPlaying);
    const playButton = document.getElementById("playpause");
    let strId = "pictureinpicture-" + (isPlaying ? "pause" : "play");
    document.l10n.setAttributes(playButton, strId);
  },

  _isMuted: false,
  /**
   * isMuted returns true if the video is currently muted.
   *
   * @return Boolean
   */
  get isMuted() {
    return this._isMuted;
  },

  /**
   * Set isMuted to true if the video is muted, false otherwise. This will
   * update the internal state and displayed controls.
   */
  set isMuted(isMuted) {
    this._isMuted = isMuted;
    this.controls.classList.toggle("muted", isMuted);
    const audioButton = document.getElementById("audio");
    let strId = "pictureinpicture-" + (isMuted ? "unmute" : "mute");
    document.l10n.setAttributes(audioButton, strId);
  },

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
   * @param revealIndefinitely (Boolean)
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
   * @param width (Number)
   *   The width of the video being played.
   * @param height (Number)
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
