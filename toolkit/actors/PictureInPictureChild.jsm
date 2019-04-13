/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PictureInPictureChild", "PictureInPictureToggleChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

ChromeUtils.defineModuleGetter(this, "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm");
ChromeUtils.defineModuleGetter(this, "DOMLocalization",
  "resource://gre/modules/DOMLocalization.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const TOGGLE_STYLESHEET = "chrome://global/skin/pictureinpicture/toggle.css";
const TOGGLE_ID = "picture-in-picture-toggle";
const FLYOUT_TOGGLE_ID = "picture-in-picture-flyout-toggle";
const FLYOUT_TOGGLE_CONTAINER = "picture-in-picture-flyout-container";
const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const FLYOUT_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.flyout-enabled";
const FLYOUT_WAIT_MS_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.flyout-wait-ms";
const FLYOUT_ANIMATION_RUNTIME_MS = 400;
const MOUSEMOVE_PROCESSING_DELAY_MS = 50;

// A weak reference to the most recent <video> in this content
// process that is being viewed in Picture-in-Picture.
var gWeakVideo = null;
// A weak reference to the content window of the most recent
// Picture-in-Picture window for this content process.
var gWeakPlayerContent = null;
// A process-global Promise that's set the first time the string for the
// flyout toggle label is requested from Fluent.
var gFlyoutLabelPromise = null;
// A process-global for the width of the toggle icon. We stash this here after
// computing it the first time to avoid repeatedly flushing styles.
var gToggleWidth = 0;

/**
 * The PictureInPictureToggleChild is responsible for displaying the overlaid
 * Picture-in-Picture toggle over top of <video> elements that the mouse is
 * hovering.
 *
 * It's also responsible for showing the "flyout" version of the toggle, which
 * currently displays on the first visible video per page.
 */
class PictureInPictureToggleChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);
    // We need to maintain some state about various things related to the
    // Picture-in-Picture toggles - however, for now, the same
    // PictureInPictureToggleChild might be re-used for different documents.
    // We keep the state stashed inside of this WeakMap, keyed on the document
    // itself.
    this.weakDocStates = new WeakMap();
    this.toggleEnabled = Services.prefs.getBoolPref(TOGGLE_ENABLED_PREF);
    this.flyoutEnabled = Services.prefs.getBoolPref(FLYOUT_ENABLED_PREF);
    this.flyoutWaitMs = Services.prefs.getIntPref(FLYOUT_WAIT_MS_PREF);

    this.l10n = new DOMLocalization([
      "toolkit/global/videocontrols.ftl",
    ]);
  }

  /**
   * Returns the state for the current document referred to via
   * this.content.document. If no such state exists, creates it, stores it
   * and returns it.
   */
  get docState() {
    let state = this.weakDocStates.get(this.content.document);
    if (!state) {
      state = {
        // A reference to the IntersectionObserver that's monitoring for videos
        // to become visible.
        intersectionObserver: null,
        // A WeakSet of videos that are supposedly visible, according to the
        // IntersectionObserver.
        weakVisibleVideos: new WeakSet(),
        // The number of videos that are supposedly visible, according to the
        // IntersectionObserver
        visibleVideos: 0,
        // The DeferredTask that we'll arm every time a mousemove event occurs
        // on a page where we have one or more visible videos.
        mousemoveDeferredTask: null,
        // A weak reference to the last video we displayed the toggle over.
        weakOverVideo: null,
        // A reference to the AnonymousContent returned after inserting the
        // small toggle.
        pipToggle: null,
        // A reference to the AnonymousContent returned after inserting the
        // flyout toggle.
        flyoutToggle: null,
      };
      this.weakDocStates.set(this.content.document, state);
    }

    return state;
  }

  handleEvent(event) {
    switch (event.type) {
      case "canplay": {
        if (this.toggleEnabled &&
            event.target instanceof this.content.HTMLVideoElement &&
            event.target.ownerDocument == this.content.document) {
          this.registerVideo(event.target);
        }
        break;
      }
      case "click": {
        let state = this.docState;
        let clickedFlyout = state.flyoutToggle &&
          state.flyoutToggle.getTargetIdForEvent(event) == FLYOUT_TOGGLE_ID;
        let clickedToggle = state.pipToggle &&
          state.pipToggle.getTargetIdForEvent(event) == TOGGLE_ID;

        if (clickedFlyout || clickedToggle) {
          let video = state.weakOverVideo && state.weakOverVideo.get();
          if (video) {
            let pipEvent =
              new this.content.CustomEvent("MozTogglePictureInPicture", {
                bubbles: true,
              });
            video.dispatchEvent(pipEvent);
            this.hideFlyout();
            this.onMouseLeaveVideo(video);
          }
        }
        break;
      }
      case "mousemove": {
        this.onMouseMove(event);
        break;
      }
    }
  }

  /**
   * Adds a <video> to the IntersectionObserver so that we know when it becomes
   * visible.
   *
   * @param {Element} video The <video> element to register.
   */
  registerVideo(video) {
    let state = this.docState;
    if (!state.intersectionObserver) {
      let fn = this.onIntersection.bind(this);
      state.intersectionObserver = new this.content.IntersectionObserver(fn, {
        threshold: [0.0, 1.0],
      });
    }

    state.intersectionObserver.observe(video);
  }

  /**
   * Called by the IntersectionObserver callback once a video becomes visible.
   * This adds some fine-grained checking to ensure that a sufficient amount of
   * the video is visible before we consider showing the toggles on it. For now,
   * that means that the entirety of the video must be in the viewport.
   *
   * @param {IntersectionEntry} intersectionEntry An IntersectionEntry passed to
   * the IntersectionObserver callback.
   * @return bool Whether or not we should start tracking mousemove events for
   * this registered video.
   */
  worthTracking(intersectionEntry) {
    let video = intersectionEntry.target;
    let rect = video.ownerGlobal.windowUtils.getBoundsWithoutFlushing(video);
    let intRect = intersectionEntry.intersectionRect;

    return intersectionEntry.isIntersecting &&
           rect.width == intRect.width &&
           rect.height == intRect.height;
  }

  /**
   * Called by the IntersectionObserver once a video crosses one of the
   * thresholds dictated by the IntersectionObserver configuration.
   *
   * @param {Array<IntersectionEntry>} A collection of one or more
   * IntersectionEntry's for <video> elements that might have entered or exited
   * the viewport.
   */
  onIntersection(entries) {
    // The IntersectionObserver will also fire when a previously intersecting
    // element is removed from the DOM. We know, however, that the node is
    // still alive and referrable from the WeakSet because the
    // IntersectionObserverEntry holds a strong reference to the video.
    let state = this.docState;
    let oldVisibleVideos = state.visibleVideos;
    for (let entry of entries) {
      let video = entry.target;
      if (this.worthTracking(entry)) {
        if (!state.weakVisibleVideos.has(video)) {
          state.weakVisibleVideos.add(video);
          state.visibleVideos++;

          // The very first video that we notice is worth tracking, we'll show
          // the flyout toggle on.
          if (this.flyoutEnabled) {
            this.content.requestIdleCallback(() => {
              this.maybeShowFlyout(video);
            });
          }
        }
      } else if (state.weakVisibleVideos.has(video)) {
        state.weakVisibleVideos.delete(video);
        state.visibleVideos--;
      }
    }

    if (!oldVisibleVideos && state.visibleVideos) {
      this.content.requestIdleCallback(() => {
        this.beginTrackingMouseOverVideos();
      });
    } else if (oldVisibleVideos && !state.visibleVideos) {
      this.content.requestIdleCallback(() => {
        this.stopTrackingMouseOverVideos();
      });
    }
  }

  /**
   * One of the challenges of displaying this toggle is that many sites put
   * things over top of <video> elements, like custom controls, or images, or
   * all manner of things that might intercept mouseevents that would normally
   * fire directly on the <video>. In order to properly detect when the mouse
   * is over top of one of the <video> elements in this situation, we currently
   * add a mousemove event handler to the entire document, and stash the most
   * recent mousemove that fires. At periodic intervals, that stashed mousemove
   * event is checked to see if it's hovering over one of our registered
   * <video> elements.
   *
   * This sort of thing will not be necessary once bug 1539652 is fixed.
   */
  beginTrackingMouseOverVideos() {
    let state = this.docState;
    if (!state.mousemoveDeferredTask) {
      state.mousemoveDeferredTask = new DeferredTask(() => {
        this.checkLastMouseMove();
      }, MOUSEMOVE_PROCESSING_DELAY_MS);
    }
    this.content.document.addEventListener("mousemove", this,
                                           { mozSystemGroup: true });
    this.content.document.addEventListener("click", this,
                                           { mozSystemGroup: true });
  }

  /**
   * If we no longer have any interesting videos in the viewport, we deregister
   * the mousemove and click listeners, and also remove any toggles that might
   * be on the page still.
   */
  stopTrackingMouseOverVideos() {
    let state = this.docState;
    state.mousemoveDeferredTask.disarm();
    this.content.document.removeEventListener("mousemove", this,
                                              { mozSystemGroup: true });
    this.content.document.removeEventListener("click", this,
                                              { mozSystemGroup: true });
    let oldOverVideo = state.weakOverVideo && state.weakOverVideo.get();
    if (oldOverVideo) {
      this.onMouseLeaveVideo(oldOverVideo);
    }
  }

  /**
   * Called for each mousemove event when we're tracking those events to
   * determine if the cursor is hovering over a <video>.
   *
   * @param {Event} event The mousemove event.
   */
  onMouseMove(event) {
    let state = this.docState;
    state.lastMouseMoveEvent = event;
    state.mousemoveDeferredTask.arm();
  }

  /**
   * Called by the DeferredTask after MOUSEMOVE_PROCESSING_DELAY_MS
   * milliseconds. Checked to see if that mousemove happens to be overtop of
   * any interesting <video> elements that we want to display the toggle
   * on. If so, puts the toggle on that video.
   */
  checkLastMouseMove() {
    let state = this.docState;
    let event = state.lastMouseMoveEvent;
    let { clientX, clientY } = event;
    let winUtils = this.content.windowUtils;
    // We use winUtils.nodesFromRect instead of document.elementsFromPoint,
    // since document.elementsFromPoint always flushes layout. The 1's in that
    // function call are for the size of the rect that we want, which is 1x1.
    let elements = winUtils.nodesFromRect(clientX, clientY, 1, 1, 1, 1, true,
                                          false);

    for (let element of elements) {
      if (state.weakVisibleVideos.has(element) &&
          !element.isCloningElementVisually) {
        this.onMouseOverVideo(element);
        return;
      }
    }

    let oldOverVideo = state.weakOverVideo && state.weakOverVideo.get();
    if (oldOverVideo) {
      this.onMouseLeaveVideo(oldOverVideo);
    }
  }

  /**
   * Called once it has been determined that the mouse is overtop of a video
   * that is in the viewport.
   *
   * @param {Element} video The video the mouse is over.
   */
  onMouseOverVideo(video) {
    let state = this.docState;
    let oldOverVideo = state.weakOverVideo && state.weakOverVideo.get();
    if (oldOverVideo && oldOverVideo == video) {
      return;
    }

    state.weakOverVideo = Cu.getWeakReference(video);
    this.moveToggleToVideo(video);
  }

  /**
   * Called once it has been determined that the mouse is no longer overlapping
   * a video that we'd previously called onMouseOverVideo with.
   *
   * @param {Element} video The video that the mouse left.
   */
  onMouseLeaveVideo(video) {
    let state = this.docState;
    state.weakOverVideo = null;
    state.pipToggle.setAttributeForElement(TOGGLE_ID, "hidden", "true");
  }

  /**
   * The toggle is injected as AnonymousContent that is positioned absolutely.
   * This method takes the <video> that we want to display the toggle on and
   * calculates where exactly we need to position the AnonymousContent in
   * absolute coordinates.
   *
   * @param {Element} video The video to display the toggle on.
   * @param {AnonymousContent} anonymousContent The anonymousContent associated
   * with the toggle about to be shown.
   * @param {String} toggleID The ID of the toggle element with the CSS
   * variables defining the toggle width and padding.
   *
   * @return {Object} with the following properties:
   *   {Number} top The top / y coordinate.
   *   {Number} left The left / x coordinate.
   *   {Number} width The width of the toggle icon, including padding.
   */
  calculateTogglePosition(video, anonymousContent, toggleID) {
    let winUtils = this.content.windowUtils;

    let scrollX = {}, scrollY = {};
    winUtils.getScrollXY(false, scrollX, scrollY);

    let rect = winUtils.getBoundsWithoutFlushing(video);

    // For now, using AnonymousContent.getComputedStylePropertyValue causes
    // a style flush, so we'll cache the value in this content process the
    // first time we read it. See bug 1541207.
    if (!gToggleWidth) {
      let widthStr = anonymousContent.getComputedStylePropertyValue(toggleID,
        "--pip-toggle-icon-width-height");
      let paddingStr = anonymousContent.getComputedStylePropertyValue(toggleID,
        "--pip-toggle-padding");
      let iconWidth = parseInt(widthStr, 0);
      let iconPadding = parseInt(paddingStr, 0);
      gToggleWidth = iconWidth + (2 * iconPadding);
    }

    let originY = rect.top + scrollY.value;
    let originX = rect.left + scrollX.value;

    let top = originY + (rect.height / 2 - Math.round(gToggleWidth / 2));
    let left = originX + (rect.width - gToggleWidth);

    return { top, left, width: gToggleWidth };
  }

  /**
   * Puts the small "Picture-in-Picture" toggle onto the passed in video.
   *
   * @param {Element} video The video to display the toggle on.
   */
  moveToggleToVideo(video) {
    let state = this.docState;
    let winUtils = this.content.windowUtils;

    if (!state.pipToggle) {
      try {
        winUtils.loadSheetUsingURIString(TOGGLE_STYLESHEET,
                                         winUtils.AGENT_SHEET);
      } catch (e) {
        // This method can fail with NS_ERROR_INVALID_ARG if the sheet is
        // already loaded - for example, from the flyout toggle.
        if (e.result != Cr.NS_ERROR_INVALID_ARG) {
          throw e;
        }
      }
      let toggle = this.content.document.createElement("button");
      toggle.classList.add("picture-in-picture-toggle-button");
      toggle.id = TOGGLE_ID;
      let icon = this.content.document.createElement("div");
      icon.classList.add("icon");
      toggle.appendChild(icon);

      state.pipToggle = this.content.document.insertAnonymousContent(toggle);
    }

    let { top, left } = this.calculateTogglePosition(video, state.pipToggle,
                                                     TOGGLE_ID);

    let styles = `
      top: ${top}px;
      left: ${left}px;
    `;

    let toggle = state.pipToggle;
    toggle.setAttributeForElement(TOGGLE_ID, "style", styles);
    // The toggle might have been hidden after a previous appearance.
    toggle.removeAttributeForElement(TOGGLE_ID, "hidden");
  }

  /**
   * Lazy getter that returns a Promise that resolves to the flyout toggle
   * label string. Sets a process-global variable to the Promise so that
   * subsequent calls within the same process don't cause us to go through
   * the Fluent look-up path again.
   */
  get flyoutLabel() {
    if (gFlyoutLabelPromise) {
      return gFlyoutLabelPromise;
    }

    gFlyoutLabelPromise =
      this.l10n.formatValue("picture-in-picture-flyout-toggle");
    return gFlyoutLabelPromise;
  }

  /**
   * If configured to, will display the "Picture-in-Picture" flyout toggle on
   * the passed-in video. This is an asynchronous function that handles the
   * entire lifecycle of the flyout animation. If a flyout toggle has already
   * been seen on this page, this function does nothing.
   *
   * @param {Element} video The video to display the flyout on.
   *
   * @return {Promise}
   * @resolves {undefined} Once the flyout toggle animation has completed.
   */
  async maybeShowFlyout(video) {
    let state = this.docState;

    if (state.flyoutToggle) {
      return;
    }

    let winUtils = this.content.windowUtils;

    try {
      winUtils.loadSheetUsingURIString(TOGGLE_STYLESHEET, winUtils.AGENT_SHEET);
    } catch (e) {
      // This method can fail with NS_ERROR_INVALID_ARG if the sheet is
      // already loaded.
      if (e.result != Cr.NS_ERROR_INVALID_ARG) {
        throw e;
      }
    }

    let container = this.content.document.createElement("div");
    container.id = FLYOUT_TOGGLE_CONTAINER;

    let toggle = this.content.document.createElement("button");
    toggle.classList.add("picture-in-picture-toggle-button");
    toggle.id = FLYOUT_TOGGLE_ID;

    let icon = this.content.document.createElement("div");
    icon.classList.add("icon");
    toggle.appendChild(icon);

    let label = this.content.document.createElement("span");
    label.classList.add("label");
    label.textContent = await this.flyoutLabel;
    toggle.appendChild(label);
    container.appendChild(toggle);
    state.flyoutToggle =
      this.content.document.insertAnonymousContent(container);

    let { top, left, width } =
      this.calculateTogglePosition(video, state.flyoutToggle, FLYOUT_TOGGLE_ID);

    let styles = `
      top: ${top}px;
      left: ${left}px;
    `;

    let flyout = state.flyoutToggle;
    flyout.setAttributeForElement(FLYOUT_TOGGLE_CONTAINER, "style", styles);
    let flyoutAnim = flyout.setAnimationForElement(FLYOUT_TOGGLE_ID, [
      { transform: `translateX(calc(100% - ${width}px))`, opacity: "0.2" },
      { transform: `translateX(calc(100% - ${width}px))`, opacity: "0.8" },
      { transform: "translateX(0)", opacity: "1" },
    ], FLYOUT_ANIMATION_RUNTIME_MS);

    await flyoutAnim.finished;

    await new Promise(resolve => this.content.setTimeout(resolve,
                                                         this.flyoutWaitMs));

    flyoutAnim.reverse();
    await flyoutAnim.finished;

    this.hideFlyout();
  }

  /**
   * Once the flyout has finished animating, or Picture-in-Picture has been
   * requested, this function can be called to hide it.
   */
  hideFlyout() {
    let state = this.docState;
    let flyout = state.flyoutToggle;
    if (flyout) {
      flyout.setAttributeForElement(FLYOUT_TOGGLE_CONTAINER, "hidden", "true");
    }
  }
}

class PictureInPictureChild extends ActorChild {
  static videoIsPlaying(video) {
    return !!(video.currentTime > 0 && !video.paused && !video.ended && video.readyState > 2);
  }

  handleEvent(event) {
    switch (event.type) {
      case "MozTogglePictureInPicture": {
        if (event.isTrusted) {
          this.togglePictureInPicture(event.target);
        }
        break;
      }
      case "pagehide": {
        // The originating video's content document has unloaded,
        // so close Picture-in-Picture.
        this.closePictureInPicture();
        break;
      }
      case "play": {
        this.mm.sendAsyncMessage("PictureInPicture:Playing");
        break;
      }
      case "pause": {
        this.mm.sendAsyncMessage("PictureInPicture:Paused");
        break;
      }
    }
  }

  get weakVideo() {
    if (gWeakVideo) {
      return gWeakVideo.get();
    }
    return null;
  }

  get weakPlayerContent() {
    if (gWeakPlayerContent) {
      return gWeakPlayerContent.get();
    }
    return null;
  }

  /**
   * Tells the parent to open a Picture-in-Picture window hosting
   * a clone of the passed video. If we know about a pre-existing
   * Picture-in-Picture window existing, this tells the parent to
   * close it before opening the new one.
   *
   * @param {Element} video The <video> element to view in a Picture
   * in Picture window.
   *
   * @return {Promise}
   * @resolves {undefined} Once the new Picture-in-Picture window
   * has been requested.
   */
  async togglePictureInPicture(video) {
    if (this.inPictureInPicture(video)) {
      await this.closePictureInPicture();
    } else {
      if (this.weakVideo) {
        // There's a pre-existing Picture-in-Picture window for a video
        // in this content process. Send a message to the parent to close
        // the Picture-in-Picture window.
        await this.closePictureInPicture();
      }

      gWeakVideo = Cu.getWeakReference(video);
      this.mm.sendAsyncMessage("PictureInPicture:Request", {
        playing: PictureInPictureChild.videoIsPlaying(video),
        videoHeight: video.videoHeight,
        videoWidth: video.videoWidth,
      });
    }
  }

  /**
   * Returns true if the passed video happens to be the one that this
   * content process is running in a Picture-in-Picture window.
   *
   * @param {Element} video The <video> element to check.
   *
   * @return {Boolean}
   */
  inPictureInPicture(video) {
    return this.weakVideo === video;
  }

  /**
   * Tells the parent to close a pre-existing Picture-in-Picture
   * window.
   *
   * @return {Promise}
   *
   * @resolves {undefined} Once the pre-existing Picture-in-Picture
   * window has unloaded.
   */
  async closePictureInPicture() {
    if (this.weakVideo) {
      this.untrackOriginatingVideo(this.weakVideo);
    }

    this.mm.sendAsyncMessage("PictureInPicture:Close", {
      browingContextId: this.docShell.browsingContext.id,
    });

    if (this.weakPlayerContent) {
      await new Promise(resolve => {
        this.weakPlayerContent.addEventListener("unload", resolve,
                                                { once: true });
      });
      // Nothing should be holding a reference to the Picture-in-Picture
      // player window content at this point, but just in case, we'll
      // clear the weak reference directly so nothing else can get a hold
      // of it from this angle.
      gWeakPlayerContent = null;
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "PictureInPicture:SetupPlayer": {
        this.setupPlayer();
        break;
      }
      case "PictureInPicture:Play": {
        this.play();
        break;
      }
      case "PictureInPicture:Pause": {
        this.pause();
        break;
      }
    }
  }

  /**
   * Keeps an eye on the originating video's document. If it ever
   * goes away, this will cause the Picture-in-Picture window for any
   * of its content to go away as well.
   */
  trackOriginatingVideo(originatingVideo) {
    let originatingWindow = originatingVideo.ownerGlobal;
    if (originatingWindow) {
      originatingWindow.addEventListener("pagehide", this);
      originatingVideo.addEventListener("play", this);
      originatingVideo.addEventListener("pause", this);
    }
  }

  /**
   * Stops tracking the originating video's document. This should
   * happen once the Picture-in-Picture window goes away (or is about
   * to go away), and we no longer care about hearing when the originating
   * window's document unloads.
   */
  untrackOriginatingVideo(originatingVideo) {
    let originatingWindow = originatingVideo.ownerGlobal;
    if (originatingWindow) {
      originatingWindow.removeEventListener("pagehide", this);
      originatingVideo.removeEventListener("play", this);
      originatingVideo.removeEventListener("pause", this);
    }
  }

  /**
   * Runs in an instance of PictureInPictureChild for the
   * player window's content, and not the originating video
   * content. Sets up the player so that it clones the originating
   * video. If anything goes wrong during set up, a message is
   * sent to the parent to close the Picture-in-Picture window.
   *
   * @return {Promise}
   * @resolves {undefined} Once the player window has been set up
   * properly, or a pre-existing Picture-in-Picture window has gone
   * away due to an unexpected error.
   */
  async setupPlayer() {
    let originatingVideo = this.weakVideo;
    if (!originatingVideo) {
      // If the video element has gone away before we've had a chance to set up
      // Picture-in-Picture for it, tell the parent to close the Picture-in-Picture
      // window.
      await this.closePictureInPicture();
      return;
    }

    let webProgress = this.mm
                          .docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebProgress);
    if (webProgress.isLoadingDocument) {
      await new Promise(resolve => {
        this.mm.addEventListener("load", resolve, {
          once: true,
          mozSystemGroup: true,
          capture: true,
        });
      });
    }

    let doc = this.content.document;
    let playerVideo = originatingVideo.cloneNode();
    playerVideo.removeAttribute("controls");

    // Mute the video and rely on the originating video's audio playback.
    // This way, we sidestep the AutoplayPolicy blocking stuff.
    playerVideo.muted = true;

    // Force the player video to assume maximum height and width of the
    // containing window
    playerVideo.style.height = "100vh";
    playerVideo.style.width = "100vw";

    // And now try to get rid of as much surrounding whitespace as possible.
    playerVideo.style.margin = "0";
    doc.body.style.overflow = "hidden";
    doc.body.style.margin = "0";

    doc.body.appendChild(playerVideo);

    originatingVideo.cloneElementVisually(playerVideo);

    this.trackOriginatingVideo(originatingVideo);

    this.content.addEventListener("unload", () => {
      if (this.weakVideo) {
        this.weakVideo.stopCloningElementVisually();
      }
      gWeakVideo = null;
    }, { once: true });

    gWeakPlayerContent = Cu.getWeakReference(this.content);
  }

  play() {
    let video = this.weakVideo;
    if (video) {
      video.play();
    }
  }

  pause() {
    let video = this.weakVideo;
    if (video) {
      video.pause();
    }
  }
}
