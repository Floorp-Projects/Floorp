/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PictureInPictureChild", "PictureInPictureToggleChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["InspectorUtils"]);

const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const TOGGLE_TESTING_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.testing";
const MOUSEMOVE_PROCESSING_DELAY_MS = 50;
const TOGGLE_HIDING_TIMEOUT_MS = 2000;

// A weak reference to the most recent <video> in this content
// process that is being viewed in Picture-in-Picture.
var gWeakVideo = null;
// A weak reference to the content window of the most recent
// Picture-in-Picture window for this content process.
var gWeakPlayerContent = null;
// To make it easier to write tests, we have a process-global
// WeakSet of all <video> elements that are being tracked for
// mouseover
var gWeakIntersectingVideosForTesting = new WeakSet();

/**
 * The PictureInPictureToggleChild is responsible for displaying the overlaid
 * Picture-in-Picture toggle over top of <video> elements that the mouse is
 * hovering.
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

    Services.prefs.addObserver(TOGGLE_ENABLED_PREF, this);
    this.toggleTesting = Services.prefs.getBoolPref(TOGGLE_TESTING_PREF, false);
  }

  cleanup() {
    this.removeMouseButtonListeners();
    Services.prefs.removeObserver(TOGGLE_ENABLED_PREF, this);
  }

  observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == TOGGLE_ENABLED_PREF) {
      this.toggleEnabled = Services.prefs.getBoolPref(TOGGLE_ENABLED_PREF);

      if (this.toggleEnabled) {
        // We have enabled the Picture-in-Picture toggle, so we need to make
        // sure we register all of the videos that might already be on the page.
        this.content.requestIdleCallback(() => {
          let videos = this.content.document.querySelectorAll("video");
          for (let video of videos) {
            this.registerVideo(video);
          }
        });
      }
    }
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
        visibleVideosCount: 0,
        // The DeferredTask that we'll arm every time a mousemove event occurs
        // on a page where we have one or more visible videos.
        mousemoveDeferredTask: null,
        // A weak reference to the last video we displayed the toggle over.
        weakOverVideo: null,
        // True if the user is in the midst of clicking the toggle.
        isClickingToggle: false,
        // Set to the original target element on pointerdown if the user is clicking
        // the toggle - this way, we can determine if a "click" event will need to be
        // suppressed ("click" events don't fire if a "mouseup" occurs on a different
        // element from the "pointerdown" / "mousedown" event).
        clickedElement: null,
        // This is a DeferredTask to hide the toggle after a period of mouse
        // inactivity.
        hideToggleDeferredTask: null,
      };
      this.weakDocStates.set(this.content.document, state);
    }

    return state;
  }

  handleEvent(event) {
    if (!event.isTrusted) {
      // We don't care about synthesized events that might be coming from
      // content JS.
      return;
    }

    switch (event.type) {
      case "canplay": {
        if (
          this.toggleEnabled &&
          event.target instanceof this.content.HTMLVideoElement &&
          event.target.ownerDocument == this.content.document
        ) {
          this.registerVideo(event.target);
        }
        break;
      }
      case "contextmenu": {
        if (this.toggleEnabled) {
          this.checkContextMenu(event);
        }
        break;
      }
      case "mousedown":
      case "pointerup":
      case "mouseup":
      case "click": {
        this.onMouseButtonEvent(event);
        break;
      }
      case "pointerdown": {
        this.onPointerDown(event);
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
        threshold: [0.0, 0.5],
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
    return intersectionEntry.isIntersecting;
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
    let oldVisibleVideosCount = state.visibleVideosCount;
    for (let entry of entries) {
      let video = entry.target;
      if (this.worthTracking(entry)) {
        if (!state.weakVisibleVideos.has(video)) {
          state.weakVisibleVideos.add(video);
          state.visibleVideosCount++;
          if (this.toggleTesting) {
            gWeakIntersectingVideosForTesting.add(video);
          }
        }
      } else if (state.weakVisibleVideos.has(video)) {
        state.weakVisibleVideos.delete(video);
        state.visibleVideosCount--;
        if (this.toggleTesting) {
          gWeakIntersectingVideosForTesting.delete(video);
        }
      }
    }

    // For testing, especially in debug or asan builds, we might not
    // run this idle callback within an acceptable time. While we're
    // testing, we'll bypass the idle callback performance optimization
    // and run our callbacks as soon as possible during the next idle
    // period.
    if (!oldVisibleVideosCount && state.visibleVideosCount) {
      if (this.toggleTesting) {
        this.beginTrackingMouseOverVideos();
      } else {
        this.content.requestIdleCallback(() => {
          this.beginTrackingMouseOverVideos();
        });
      }
    } else if (oldVisibleVideosCount && !state.visibleVideosCount) {
      if (this.toggleTesting) {
        this.stopTrackingMouseOverVideos();
      } else {
        this.content.requestIdleCallback(() => {
          this.stopTrackingMouseOverVideos();
        });
      }
    }
  }

  addMouseButtonListeners() {
    // We want to try to cancel the mouse events from continuing
    // on into content if the user has clicked on the toggle, so
    // we don't use the mozSystemGroup here, and add the listener
    // to the parent target of the window, which in this case,
    // is the windowRoot. Since this event listener is attached to
    // part of the outer window, we need to also remove it in a
    // pagehide event listener in the event that the page unloads
    // before stopTrackingMouseOverVideos fires.
    this.content.windowRoot.addEventListener("pointerdown", this, {
      capture: true,
    });
    this.content.windowRoot.addEventListener("mousedown", this, {
      capture: true,
    });
    this.content.windowRoot.addEventListener("mouseup", this, {
      capture: true,
    });
    this.content.windowRoot.addEventListener("pointerup", this, {
      capture: true,
    });
    this.content.windowRoot.addEventListener("click", this, { capture: true });
  }

  removeMouseButtonListeners() {
    this.content.windowRoot.removeEventListener("pointerdown", this, {
      capture: true,
    });
    this.content.windowRoot.removeEventListener("mousedown", this, {
      capture: true,
    });
    this.content.windowRoot.removeEventListener("mouseup", this, {
      capture: true,
    });
    this.content.windowRoot.removeEventListener("pointerup", this, {
      capture: true,
    });
    this.content.windowRoot.removeEventListener("click", this, {
      capture: true,
    });
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
    this.content.document.addEventListener("mousemove", this, {
      mozSystemGroup: true,
      capture: true,
    });
    this.addMouseButtonListeners();
  }

  /**
   * If we no longer have any interesting videos in the viewport, we deregister
   * the mousemove and click listeners, and also remove any toggles that might
   * be on the page still.
   */
  stopTrackingMouseOverVideos() {
    let state = this.docState;
    state.mousemoveDeferredTask.disarm();
    this.content.document.removeEventListener("mousemove", this, {
      mozSystemGroup: true,
      capture: true,
    });
    this.removeMouseButtonListeners();
    let oldOverVideo = state.weakOverVideo && state.weakOverVideo.get();
    if (oldOverVideo) {
      this.onMouseLeaveVideo(oldOverVideo);
    }
  }

  /**
   * If we're tracking <video> elements, this pointerdown event handler is run anytime
   * a pointerdown occurs on the document. This function is responsible for checking
   * if the user clicked on the Picture-in-Picture toggle. It does this by first
   * checking if the video is visible beneath the point that was clicked. Then
   * it tests whether or not the pointerdown occurred within the rectangle of the
   * toggle. If so, the event's propagation is stopped, and Picture-in-Picture is
   * triggered.
   *
   * @param {Event} event The mousemove event.
   */
  onPointerDown(event) {
    // The toggle ignores non-primary mouse clicks.
    if (event.button != 0) {
      return;
    }

    let state = this.docState;

    let video = state.weakOverVideo && state.weakOverVideo.get();
    if (!video) {
      return;
    }

    let shadowRoot = video.openOrClosedShadowRoot;
    if (!shadowRoot) {
      return;
    }

    let { clientX, clientY } = event;
    let winUtils = this.content.windowUtils;
    // We use winUtils.nodesFromRect instead of document.elementsFromPoint,
    // since document.elementsFromPoint always flushes layout. The 1's in that
    // function call are for the size of the rect that we want, which is 1x1.
    //
    // We pass the aOnlyVisible boolean argument to check that the video isn't
    // occluded by anything visible at the point of mousedown. If it is, we'll
    // ignore the mousedown.
    let elements = winUtils.nodesFromRect(
      clientX,
      clientY,
      1,
      1,
      1,
      1,
      true,
      false,
      true /* aOnlyVisible */
    );
    if (!Array.from(elements).includes(video)) {
      return;
    }

    let toggle = shadowRoot.getElementById("pictureInPictureToggleButton");
    if (this.isMouseOverToggle(toggle, event)) {
      state.isClickingToggle = true;
      state.clickedElement = Cu.getWeakReference(event.originalTarget);
      event.stopImmediatePropagation();

      Services.telemetry.keyedScalarAdd(
        "pictureinpicture.opened_method",
        "toggle",
        1
      );

      let pipEvent = new this.content.CustomEvent("MozTogglePictureInPicture", {
        bubbles: true,
      });
      video.dispatchEvent(pipEvent);

      // Since we've initiated Picture-in-Picture, we can go ahead and
      // hide the toggle now.
      this.onMouseLeaveVideo(video);
    }
  }

  /**
   * Called for mousedown, pointerup, mouseup and click events. If we
   * detected that the user is clicking on the Picture-in-Picture toggle,
   * these events are cancelled in the capture-phase before they reach
   * content. The state for suppressing these events is cleared on the
   * click event (unless the mouseup occurs on a different element from
   * the mousedown, in which case, the state is cleared on mouseup).
   *
   * @param {Event} event A mousedown, pointerup, mouseup or click event.
   */
  onMouseButtonEvent(event) {
    // The toggle ignores non-primary mouse clicks.
    if (event.button != 0) {
      return;
    }

    let state = this.docState;
    if (state.isClickingToggle) {
      event.stopImmediatePropagation();

      // If this is a mouseup event, check to see if we have a record of what
      // the original target was on pointerdown. If so, and if it doesn't match
      // the mouseup original target, that means we won't get a click event, and
      // we can clear the "clicking the toggle" state right away.
      //
      // Otherwise, we wait for the click event to do that.
      let isMouseUpOnOtherElement =
        event.type == "mouseup" &&
        (!state.clickedElement ||
          state.clickedElement.get() != event.originalTarget);

      if (isMouseUpOnOtherElement || event.type == "click") {
        // The click is complete, so now we reset the state so that
        // we stop suppressing these events.
        state.isClickingToggle = false;
        state.clickedElement = null;
      }
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

    if (state.hideToggleDeferredTask) {
      state.hideToggleDeferredTask.disarm();
      state.hideToggleDeferredTask.arm();
    }

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
    let elements = winUtils.nodesFromRect(
      clientX,
      clientY,
      1,
      1,
      1,
      1,
      true,
      false,
      false
    );

    for (let element of elements) {
      if (
        state.weakVisibleVideos.has(element) &&
        !element.isCloningElementVisually
      ) {
        this.onMouseOverVideo(element, event);
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
  onMouseOverVideo(video, event) {
    let state = this.docState;
    let oldOverVideo = state.weakOverVideo && state.weakOverVideo.get();
    let shadowRoot = video.openOrClosedShadowRoot;

    // It seems from automated testing that if it's still very early on in the
    // lifecycle of a <video> element, it might not yet have a shadowRoot,
    // in which case, we can bail out here early.
    if (!shadowRoot) {
      if (oldOverVideo) {
        // We also clear the hover state on the old video we were hovering,
        // if there was one.
        this.onMouseLeaveVideo(oldOverVideo);
      }

      return;
    }

    let toggle = shadowRoot.getElementById("pictureInPictureToggleButton");
    let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");
    controlsOverlay.removeAttribute("hidetoggle");

    // The hideToggleDeferredTask we create here is for automatically hiding
    // the toggle after a period of no mousemove activity for
    // TOGGLE_HIDING_TIMEOUT_MS. If the mouse moves, then the DeferredTask
    // timer is reset.
    //
    // We disable the toggle hiding timeout during testing to reduce
    // non-determinism from timers when testing the toggle.
    if (!state.hideToggleDeferredTask && !this.toggleTesting) {
      state.hideToggleDeferredTask = new DeferredTask(() => {
        controlsOverlay.setAttribute("hidetoggle", true);
      }, TOGGLE_HIDING_TIMEOUT_MS);
    }

    if (oldOverVideo) {
      if (oldOverVideo == video) {
        // If we're still hovering the old video, we might have entered or
        // exited the toggle region.
        this.checkHoverToggle(toggle, event);
        return;
      }

      // We had an old video that we were hovering, and we're not hovering
      // it anymore. Let's leave it.
      this.onMouseLeaveVideo(oldOverVideo);
    }

    state.weakOverVideo = Cu.getWeakReference(video);
    InspectorUtils.addPseudoClassLock(controlsOverlay, ":hover");

    // Now that we're hovering the video, we'll check to see if we're
    // hovering the toggle too.
    this.checkHoverToggle(toggle, event);
  }

  /**
   * Checks if a mouse event is happening over a toggle element. If it is,
   * sets the :hover pseudoclass on it. Otherwise, it clears the :hover
   * pseudoclass.
   *
   * @param {Element} toggle The Picture-in-Picture toggle to check.
   * @param {MouseEvent} event A MouseEvent to test.
   */
  checkHoverToggle(toggle, event) {
    if (this.isMouseOverToggle(toggle, event)) {
      InspectorUtils.addPseudoClassLock(toggle, ":hover");
    } else {
      InspectorUtils.removePseudoClassLock(toggle, ":hover");
    }
  }

  /**
   * Called once it has been determined that the mouse is no longer overlapping
   * a video that we'd previously called onMouseOverVideo with.
   *
   * @param {Element} video The video that the mouse left.
   */
  onMouseLeaveVideo(video) {
    let state = this.docState;
    let shadowRoot = video.openOrClosedShadowRoot;

    if (shadowRoot) {
      let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");
      let toggle = shadowRoot.getElementById("pictureInPictureToggleButton");
      InspectorUtils.removePseudoClassLock(controlsOverlay, ":hover");
      InspectorUtils.removePseudoClassLock(toggle, ":hover");
    }

    state.weakOverVideo = null;
    state.hideToggleDeferredTask.disarm();
    state.hideToggleDeferredTask = null;
  }

  /**
   * Given a reference to a Picture-in-Picture toggle element, determines
   * if a MouseEvent event is occurring within its bounds.
   *
   * @param {Element} toggle The Picture-in-Picture toggle.
   * @param {MouseEvent} event A MouseEvent to test.
   *
   * @return {Boolean}
   */
  isMouseOverToggle(toggle, event) {
    let toggleRect = toggle.ownerGlobal.windowUtils.getBoundsWithoutFlushing(
      toggle
    );
    let { clientX, clientY } = event;
    return (
      clientX >= toggleRect.left &&
      clientX <= toggleRect.right &&
      clientY >= toggleRect.top &&
      clientY <= toggleRect.bottom
    );
  }

  /**
   * Checks a contextmenu event to see if the mouse is currently over the
   * Picture-in-Picture toggle. If so, sends a message to the parent process
   * to open up the Picture-in-Picture toggle context menu.
   *
   * @param {MouseEvent} event A contextmenu event.
   */
  checkContextMenu(event) {
    let state = this.docState;

    let video = state.weakOverVideo && state.weakOverVideo.get();
    if (!video) {
      return;
    }

    let shadowRoot = video.openOrClosedShadowRoot;
    if (!shadowRoot) {
      return;
    }

    let toggle = shadowRoot.getElementById("pictureInPictureToggleButton");
    if (this.isMouseOverToggle(toggle, event)) {
      event.stopImmediatePropagation();
      event.preventDefault();

      this.mm.sendAsyncMessage("PictureInPicture:OpenToggleContextMenu", {
        screenX: event.screenX,
        screenY: event.screenY,
        mozInputSource: event.mozInputSource,
      });
    }
  }

  /**
   * This is a test-only function that returns true if a video is being tracked
   * for mouseover events after having intersected the viewport.
   */
  static isTracking(video) {
    return gWeakIntersectingVideosForTesting.has(video);
  }
}

class PictureInPictureChild extends ActorChild {
  static videoIsPlaying(video) {
    return !!(
      video.currentTime > 0 &&
      !video.paused &&
      !video.ended &&
      video.readyState > 2
    );
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
        this.closePictureInPicture({ reason: "pagehide" });
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
      // The only way we could have entered here for the same video is if
      // we are toggling via the context menu, since we hide the inline
      // Picture-in-Picture toggle when a video is being displayed in
      // Picture-in-Picture.
      await this.closePictureInPicture({ reason: "contextmenu" });
    } else {
      if (this.weakVideo) {
        // There's a pre-existing Picture-in-Picture window for a video
        // in this content process. Send a message to the parent to close
        // the Picture-in-Picture window.
        await this.closePictureInPicture({ reason: "new-pip" });
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
  async closePictureInPicture({ reason }) {
    if (this.weakVideo) {
      this.untrackOriginatingVideo(this.weakVideo);
    }

    this.mm.sendAsyncMessage("PictureInPicture:Close", {
      browingContextId: this.docShell.browsingContext.id,
      reason,
    });

    if (this.weakPlayerContent) {
      if (!this.weakPlayerContent.closed) {
        await new Promise(resolve => {
          this.weakPlayerContent.addEventListener("unload", resolve, {
            once: true,
          });
        });
      }
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
      await this.closePictureInPicture({ reason: "setup-failure" });
      return;
    }

    let webProgress = this.mm.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
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
    // Clone the original video to get its MediaInfo (specifically, it's dimensions)
    // set right away, but also pause the video since we don't need two copies of it
    // playing at the same time. The originating video will be "projected" onto the
    // cloned Picture-in-Picture player video via cloneElementVisually.
    let playerVideo = originatingVideo.cloneNode();
    playerVideo.pause();
    playerVideo.removeAttribute("controls");

    // Mute the video and rely on the originating video's audio playback.
    // This way, we sidestep the AutoplayPolicy blocking stuff.
    playerVideo.muted = true;

    // Strip any inline styles off of the video, and try to get rid of any surrounding
    // whitespace.
    playerVideo.setAttribute("style", "");
    doc.body.style.overflow = "hidden";
    doc.body.style.margin = "0";

    // Force the player video to assume maximum height and width of the
    // containing window
    playerVideo.style.height = "100vh";
    playerVideo.style.width = "100vw";

    doc.body.appendChild(playerVideo);

    originatingVideo.cloneElementVisually(playerVideo);

    this.trackOriginatingVideo(originatingVideo);

    this.content.addEventListener(
      "unload",
      () => {
        if (this.weakVideo) {
          this.weakVideo.stopCloningElementVisually();
        }
        gWeakVideo = null;
      },
      { once: true }
    );

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
