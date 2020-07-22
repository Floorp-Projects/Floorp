/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PictureInPictureChild", "PictureInPictureToggleChild"];

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
ChromeUtils.defineModuleGetter(
  this,
  "TOGGLE_POLICIES",
  "resource://gre/modules/PictureInPictureTogglePolicy.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TOGGLE_POLICY_STRINGS",
  "resource://gre/modules/PictureInPictureTogglePolicy.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Rect",
  "resource://gre/modules/Geometry.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";
const TOGGLE_TESTING_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.testing";
const TOGGLE_EXPERIMENTAL_MODE_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.mode";
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

// Overrides are expected to stay constant for the lifetime of a
// content process, so we set this as a lazy process global.
// See PictureInPictureToggleChild.getToggleOverrides for a sense
// of what the return type is.
XPCOMUtils.defineLazyGetter(this, "gToggleOverrides", () => {
  return PictureInPictureToggleChild.getToggleOverrides();
});

/**
 * The PictureInPictureToggleChild is responsible for displaying the overlaid
 * Picture-in-Picture toggle over top of <video> elements that the mouse is
 * hovering.
 */
class PictureInPictureToggleChild extends JSWindowActorChild {
  constructor() {
    super();
    // We need to maintain some state about various things related to the
    // Picture-in-Picture toggles - however, for now, the same
    // PictureInPictureToggleChild might be re-used for different documents.
    // We keep the state stashed inside of this WeakMap, keyed on the document
    // itself.
    this.weakDocStates = new WeakMap();
    this.toggleEnabled = Services.prefs.getBoolPref(TOGGLE_ENABLED_PREF);
    this.toggleTesting = Services.prefs.getBoolPref(TOGGLE_TESTING_PREF, false);
    this.experimentalToggle =
      Services.prefs.getIntPref(TOGGLE_EXPERIMENTAL_MODE_PREF, -1) != -1;

    // Bug 1570744 - JSWindowActorChild's cannot be used as nsIObserver's
    // directly, so we create a new function here instead to act as our
    // nsIObserver, which forwards the notification to the observe method.
    this.observerFunction = (subject, topic, data) => {
      this.observe(subject, topic, data);
    };
    Services.prefs.addObserver(TOGGLE_ENABLED_PREF, this.observerFunction);
    Services.prefs.addObserver(
      TOGGLE_EXPERIMENTAL_MODE_PREF,
      this.observerFunction
    );
  }

  willDestroy() {
    this.stopTrackingMouseOverVideos();
    Services.prefs.removeObserver(TOGGLE_ENABLED_PREF, this.observerFunction);
    Services.prefs.removeObserver(
      TOGGLE_EXPERIMENTAL_MODE_PREF,
      this.observerFunction
    );
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed") {
      return;
    }

    switch (data) {
      case TOGGLE_ENABLED_PREF: {
        this.toggleEnabled = Services.prefs.getBoolPref(TOGGLE_ENABLED_PREF);

        if (this.toggleEnabled) {
          // We have enabled the Picture-in-Picture toggle, so we need to make
          // sure we register all of the videos that might already be on the page.
          this.contentWindow.requestIdleCallback(() => {
            let videos = this.document.querySelectorAll("video");
            for (let video of videos) {
              this.registerVideo(video);
            }
          });
        }
        break;
      }
      case TOGGLE_EXPERIMENTAL_MODE_PREF: {
        this.experimentalToggle =
          Services.prefs.getIntPref(TOGGLE_EXPERIMENTAL_MODE_PREF, -1) != -1;
        break;
      }
    }
  }

  /**
   * Returns the state for the current document referred to via
   * this.document. If no such state exists, creates it, stores it
   * and returns it.
   */
  get docState() {
    let state = this.weakDocStates.get(this.document);
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
        // If we reach a point where we're tracking videos for mouse movements,
        // then this will be true. If there are no videos worth tracking, then
        // this is false.
        isTrackingVideos: false,
        togglePolicy: TOGGLE_POLICIES.DEFAULT,
        hasCheckedPolicy: false,
      };
      this.weakDocStates.set(this.document, state);
    }

    return state;
  }

  /**
   * Returns the video that the user was last hovering with the mouse if it
   * still exists.
   *
   * @return {Element} the <video> element that the user was last hovering,
   * or null if there was no such <video>, or the <video> no longer exists.
   */
  getWeakOverVideo() {
    let { weakOverVideo } = this.docState;
    if (weakOverVideo) {
      // Bug 800957 - Accessing weakrefs at the wrong time can cause us to
      // throw NS_ERROR_XPC_BAD_CONVERT_NATIVE
      try {
        return weakOverVideo.get();
      } catch (e) {
        return null;
      }
    }
    return null;
  }

  handleEvent(event) {
    if (!event.isTrusted) {
      // We don't care about synthesized events that might be coming from
      // content JS.
      return;
    }

    if (gWeakPlayerContent) {
      try {
        if (this.contentWindow == gWeakPlayerContent.get()) {
          // We also don't want events from the player window video itself!
          return;
        }
      } catch (e) {
        return;
      }
    }

    switch (event.type) {
      case "UAWidgetSetupOrChange": {
        if (
          this.toggleEnabled &&
          event.target instanceof this.contentWindow.HTMLVideoElement &&
          event.target.ownerDocument == this.document
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
      case "mouseout": {
        this.onMouseOut(event);
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
      case "pageshow": {
        this.onPageShow(event);
        break;
      }
      case "pagehide": {
        this.onPageHide(event);
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
      state.intersectionObserver = new this.contentWindow.IntersectionObserver(
        fn,
        {
          threshold: [0.0, 0.5],
        }
      );
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
        this.contentWindow.requestIdleCallback(() => {
          this.beginTrackingMouseOverVideos();
        });
      }
    } else if (oldVisibleVideosCount && !state.visibleVideosCount) {
      if (this.toggleTesting) {
        this.stopTrackingMouseOverVideos();
      } else {
        this.contentWindow.requestIdleCallback(() => {
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
    this.contentWindow.windowRoot.addEventListener("pointerdown", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.addEventListener("mousedown", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.addEventListener("mouseup", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.addEventListener("pointerup", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.addEventListener("click", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.addEventListener("mouseout", this, {
      capture: true,
    });
  }

  removeMouseButtonListeners() {
    this.contentWindow.windowRoot.removeEventListener("pointerdown", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.removeEventListener("mousedown", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.removeEventListener("mouseup", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.removeEventListener("pointerup", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.removeEventListener("click", this, {
      capture: true,
    });
    this.contentWindow.windowRoot.removeEventListener("mouseout", this, {
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
    this.document.addEventListener("mousemove", this, {
      mozSystemGroup: true,
      capture: true,
    });
    this.contentWindow.addEventListener("pageshow", this, {
      mozSystemGroup: true,
    });
    this.contentWindow.addEventListener("pagehide", this, {
      mozSystemGroup: true,
    });
    this.addMouseButtonListeners();
    state.isTrackingVideos = true;
  }

  /**
   * If we no longer have any interesting videos in the viewport, we deregister
   * the mousemove and click listeners, and also remove any toggles that might
   * be on the page still.
   */
  stopTrackingMouseOverVideos() {
    let state = this.docState;
    // We initialize `mousemoveDeferredTask` in `beginTrackingMouseOverVideos`.
    // If it doesn't exist, that can't have happened. Nothing else ever sets
    // this value (though we arm/disarm in various places). So we don't need
    // to do anything else here and can return early.
    if (!state.mousemoveDeferredTask) {
      return;
    }
    state.mousemoveDeferredTask.disarm();
    this.document.removeEventListener("mousemove", this, {
      mozSystemGroup: true,
      capture: true,
    });
    this.contentWindow.removeEventListener("pageshow", this, {
      mozSystemGroup: true,
    });
    this.contentWindow.removeEventListener("pagehide", this, {
      mozSystemGroup: true,
    });
    this.removeMouseButtonListeners();
    let oldOverVideo = this.getWeakOverVideo();
    if (oldOverVideo) {
      this.onMouseLeaveVideo(oldOverVideo);
    }
    state.isTrackingVideos = false;
  }

  /**
   * This pageshow event handler will get called if and when we complete a tab
   * tear out or in. If we happened to be tracking videos before the tear
   * occurred, we re-add the mouse event listeners so that they're attached to
   * the right WindowRoot.
   *
   * @param {Event} event The pageshow event fired when completing a tab tear
   * out or in.
   */
  onPageShow(event) {
    let state = this.docState;
    if (state.isTrackingVideos) {
      this.addMouseButtonListeners();
    }
  }

  /**
   * This pagehide event handler will get called if and when we start a tab
   * tear out or in. If we happened to be tracking videos before the tear
   * occurred, we remove the mouse event listeners. We'll re-add them when the
   * pageshow event fires.
   *
   * @param {Event} event The pagehide event fired when starting a tab tear
   * out or in.
   */
  onPageHide(event) {
    let state = this.docState;
    if (state.isTrackingVideos) {
      this.removeMouseButtonListeners();
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

    let video = this.getWeakOverVideo();
    if (!video) {
      return;
    }

    let shadowRoot = video.openOrClosedShadowRoot;
    if (!shadowRoot) {
      return;
    }

    let { clientX, clientY } = event;
    let winUtils = this.contentWindow.windowUtils;
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

    let toggle = this.getToggleElement(shadowRoot);
    if (this.isMouseOverToggle(toggle, event)) {
      let state = this.docState;
      state.isClickingToggle = true;
      state.clickedElement = Cu.getWeakReference(event.originalTarget);
      event.stopImmediatePropagation();

      Services.telemetry.keyedScalarAdd(
        "pictureinpicture.opened_method",
        "toggle",
        1
      );

      let pipEvent = new this.contentWindow.CustomEvent(
        "MozTogglePictureInPicture",
        {
          bubbles: true,
        }
      );
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
   * Called on mouseout events to determine whether or not the mouse has
   * exited the window.
   *
   * @param {Event} event The mouseout event.
   */
  onMouseOut(event) {
    if (!event.relatedTarget) {
      // For mouseout events, if there's no relatedTarget (which normally
      // maps to the element that the mouse entered into) then this means that
      // we left the window.
      let video = this.getWeakOverVideo();
      if (!video) {
        return;
      }

      this.onMouseLeaveVideo(video);
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
    let winUtils = this.contentWindow.windowUtils;
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
      true
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

    let oldOverVideo = this.getWeakOverVideo();
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
    let oldOverVideo = this.getWeakOverVideo();
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

    let state = this.docState;
    let toggle = this.getToggleElement(shadowRoot);
    let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");

    if (!state.hasCheckedPolicy) {
      // We cache the matchers process-wide. We'll skip this while running tests to make that
      // easier.
      let toggleOverrides = this.toggleTesting
        ? PictureInPictureToggleChild.getToggleOverrides()
        : gToggleOverrides;

      // Do we have any toggle overrides? If so, try to apply them.
      for (let [override, policy] of toggleOverrides) {
        if (override.matches(this.document.documentURI)) {
          state.togglePolicy = policy;
          break;
        }
      }

      state.hasCheckedPolicy = true;
    }

    // The built-in <video> controls are along the bottom, which would overlap the
    // toggle if the override is set to BOTTOM, so we ignore overrides that set
    // a policy of BOTTOM for <video> elements with controls.
    if (
      state.togglePolicy != TOGGLE_POLICIES.DEFAULT &&
      !(state.togglePolicy == TOGGLE_POLICIES.BOTTOM && video.controls)
    ) {
      toggle.setAttribute("policy", TOGGLE_POLICY_STRINGS[state.togglePolicy]);
    }

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
    controlsOverlay.classList.add("hovering");

    // Now that we're hovering the video, we'll check to see if we're
    // hovering the toggle too.
    this.checkHoverToggle(toggle, event);
  }

  /**
   * Checks if a mouse event is happening over a toggle element. If it is,
   * sets the hovering class on it. Otherwise, it clears the hovering
   * class.
   *
   * @param {Element} toggle The Picture-in-Picture toggle to check.
   * @param {MouseEvent} event A MouseEvent to test.
   */
  checkHoverToggle(toggle, event) {
    toggle.classList.toggle("hovering", this.isMouseOverToggle(toggle, event));
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
      let toggle = this.getToggleElement(shadowRoot);
      controlsOverlay.classList.remove("hovering");
      toggle.classList.remove("hovering");
    }

    state.weakOverVideo = null;

    if (!this.toggleTesting) {
      state.hideToggleDeferredTask.disarm();
      state.mousemoveDeferredTask.disarm();
    }

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

    if (this.experimentalToggle) {
      // The way the experimental toggles are currently implemented with
      // absolute positioning, the root toggle element bounds don't actually
      // contain all of the toggle child element bounds. Until we find a way to
      // sort that out, we workaround the issue by having each clickable child
      // elements of the toggle have a clicklable class, and then compute the
      // smallest rect that contains all of their bounding rects and use that
      // as the hitbox.
      toggleRect = Rect.fromRect(toggleRect);
      let clickableChildren = toggle.querySelectorAll(".clickable");
      for (let child of clickableChildren) {
        let childRect = Rect.fromRect(
          child.ownerGlobal.windowUtils.getBoundsWithoutFlushing(child)
        );
        toggleRect.expandToContain(childRect);
      }
    }

    // If the toggle has no dimensions, we're definitely not over it.
    if (!toggleRect.width || !toggleRect.height) {
      return false;
    }

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
    let video = this.getWeakOverVideo();
    if (!video) {
      return;
    }

    let shadowRoot = video.openOrClosedShadowRoot;
    if (!shadowRoot) {
      return;
    }

    let toggle = this.getToggleElement(shadowRoot);
    if (this.isMouseOverToggle(toggle, event)) {
      event.stopImmediatePropagation();
      event.preventDefault();

      this.sendAsyncMessage("PictureInPicture:OpenToggleContextMenu", {
        screenX: event.screenX,
        screenY: event.screenY,
        mozInputSource: event.mozInputSource,
      });
    }
  }

  /**
   * Returns the appropriate root element for the Picture-in-Picture toggle,
   * depending on whether or not we're using the experimental toggle preference.
   *
   * @param {Element} shadowRoot The shadowRoot of the video element.
   * @returns {Element} The toggle element.
   */
  getToggleElement(shadowRoot) {
    if (!this.experimentalToggle) {
      return shadowRoot.getElementById("pictureInPictureToggleButton");
    }
    return shadowRoot.getElementById("pictureInPictureToggleExperiment");
  }

  /**
   * This is a test-only function that returns true if a video is being tracked
   * for mouseover events after having intersected the viewport.
   */
  static isTracking(video) {
    return gWeakIntersectingVideosForTesting.has(video);
  }

  /**
   * Gets any Picture-in-Picture toggle overrides stored in the sharedData
   * struct, and returns them as an Array of two-element Arrays, where the first
   * element is a MatchPattern and the second element is a policy.
   *
   * @returns {Array<Array<2>>} Array of 2-element Arrays where the first element
   * is a MatchPattern and the second element is a Number representing a toggle
   * policy.
   */
  static getToggleOverrides() {
    let result = [];
    let patterns = Services.cpmm.sharedData.get(
      "PictureInPicture:ToggleOverrides"
    );
    for (let pattern in patterns) {
      let matcher = new MatchPattern(pattern);
      result.push([matcher, patterns[pattern]]);
    }
    return result;
  }
}

class PictureInPictureChild extends JSWindowActorChild {
  static videoIsPlaying(video) {
    return !!(
      video.currentTime > 0 &&
      !video.paused &&
      !video.ended &&
      video.readyState > 2
    );
  }

  static videoIsMuted(video) {
    return video.muted;
  }

  handleEvent(event) {
    switch (event.type) {
      case "MozTogglePictureInPicture": {
        if (event.isTrusted) {
          this.togglePictureInPicture(event.target);
        }
        break;
      }
      case "MozStopPictureInPicture": {
        if (event.isTrusted && event.target === this.getWeakVideo()) {
          this.closePictureInPicture({ reason: "video-el-remove" });
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
        this.sendAsyncMessage("PictureInPicture:Playing");
        break;
      }
      case "pause": {
        this.sendAsyncMessage("PictureInPicture:Paused");
        break;
      }
      case "volumechange": {
        let video = this.getWeakVideo();

        // Just double-checking that we received the event for the right
        // video element.
        if (video !== event.target) {
          Cu.reportError(
            "PictureInPictureChild received volumechange for " +
              "the wrong video!"
          );
          return;
        }

        if (video.muted) {
          this.sendAsyncMessage("PictureInPicture:Muting");
        } else {
          this.sendAsyncMessage("PictureInPicture:Unmuting");
        }
        break;
      }
      case "resize": {
        let video = event.target;
        if (this.inPictureInPicture(video)) {
          this.sendAsyncMessage("PictureInPicture:Resize", {
            videoHeight: video.videoHeight,
            videoWidth: video.videoWidth,
          });
        }
        break;
      }
    }
  }

  /**
   * Returns a reference to the <video> element being displayed in Picture-in-Picture
   * mode.
   *
   * @return {Element} The <video> being displayed in Picture-in-Picture mode, or null
   * if that <video> no longer exists.
   */
  getWeakVideo() {
    if (gWeakVideo) {
      // Bug 800957 - Accessing weakrefs at the wrong time can cause us to
      // throw NS_ERROR_XPC_BAD_CONVERT_NATIVE
      try {
        return gWeakVideo.get();
      } catch (e) {
        return null;
      }
    }
    return null;
  }

  /**
   * Returns a reference to the inner window of the about:blank document that is
   * cloning the originating <video> in the always-on-top player <xul:browser>.
   *
   * @return {Window} The inner window of the about:blank player <xul:browser>, or
   * null if that window has been closed.
   */
  getWeakPlayerContent() {
    if (gWeakPlayerContent) {
      // Bug 800957 - Accessing weakrefs at the wrong time can cause us to
      // throw NS_ERROR_XPC_BAD_CONVERT_NATIVE
      try {
        return gWeakPlayerContent.get();
      } catch (e) {
        return null;
      }
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
    // We don't allow viewing <video> elements with MediaStreams
    // in Picture-in-Picture for now due to bug 1592539.
    if (video.srcObject) {
      return;
    }

    if (this.inPictureInPicture(video)) {
      // The only way we could have entered here for the same video is if
      // we are toggling via the context menu, since we hide the inline
      // Picture-in-Picture toggle when a video is being displayed in
      // Picture-in-Picture.
      await this.closePictureInPicture({ reason: "contextmenu" });
    } else {
      if (this.getWeakVideo()) {
        // There's a pre-existing Picture-in-Picture window for a video
        // in this content process. Send a message to the parent to close
        // the Picture-in-Picture window.
        await this.closePictureInPicture({ reason: "new-pip" });
      }

      gWeakVideo = Cu.getWeakReference(video);
      this.sendAsyncMessage("PictureInPicture:Request", {
        isMuted: PictureInPictureChild.videoIsMuted(video),
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
    return this.getWeakVideo() === video;
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
    let video = this.getWeakVideo();
    if (video) {
      this.untrackOriginatingVideo(video);
    }
    this.sendAsyncMessage("PictureInPicture:Close", {
      reason,
    });

    let playerContent = this.getWeakPlayerContent();
    if (playerContent) {
      if (!playerContent.closed) {
        await new Promise(resolve => {
          playerContent.addEventListener("unload", resolve, {
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
      case "PictureInPicture:Mute": {
        this.mute();
        break;
      }
      case "PictureInPicture:Unmute": {
        this.unmute();
        break;
      }
      case "PictureInPicture:KeyDown": {
        this.keyDown(message.data);
        break;
      }
      case "PictureInPicture:KeyToggle": {
        this.keyToggle();
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
      originatingVideo.addEventListener("volumechange", this);
      originatingVideo.addEventListener("resize", this);
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
      originatingVideo.removeEventListener("volumechange", this);
      originatingVideo.removeEventListener("resize", this);
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
    let originatingVideo = this.getWeakVideo();
    if (!originatingVideo) {
      // If the video element has gone away before we've had a chance to set up
      // Picture-in-Picture for it, tell the parent to close the Picture-in-Picture
      // window.
      await this.closePictureInPicture({ reason: "setup-failure" });
      return;
    }

    this.contentWindow.location.reload();
    let webProgress = this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    if (webProgress.isLoadingDocument) {
      await new Promise(resolve => {
        this.contentWindow.addEventListener("load", resolve, {
          once: true,
          mozSystemGroup: true,
          capture: true,
        });
      });
    }

    // We're committed to adding the video to this window now. Ensure we track
    // the content window before we do so, so that the toggle actor can
    // distinguish this new video we're creating from web-controlled ones.
    gWeakPlayerContent = Cu.getWeakReference(this.contentWindow);

    let doc = this.document;
    let playerVideo = doc.createElement("video");

    doc.body.style.overflow = "hidden";
    doc.body.style.margin = "0";

    // Force the player video to assume maximum height and width of the
    // containing window
    playerVideo.style.height = "100vh";
    playerVideo.style.width = "100vw";
    playerVideo.style.backgroundColor = "#000";

    doc.body.appendChild(playerVideo);

    originatingVideo.cloneElementVisually(playerVideo);

    this.trackOriginatingVideo(originatingVideo);

    this.contentWindow.addEventListener(
      "unload",
      () => {
        let video = this.getWeakVideo();
        if (video) {
          this.untrackOriginatingVideo(video);
          video.stopCloningElementVisually();
        }
        gWeakVideo = null;
      },
      { once: true }
    );
  }

  play() {
    let video = this.getWeakVideo();
    if (video) {
      video.play();
    }
  }

  pause() {
    let video = this.getWeakVideo();
    if (video) {
      video.pause();
    }
  }

  mute() {
    let video = this.getWeakVideo();
    if (video) {
      video.muted = true;
    }
  }

  unmute() {
    let video = this.getWeakVideo();
    if (video) {
      video.muted = false;
    }
  }

  /**
   * This reuses the keyHandler logic in the VideoControlsWidget
   * https://searchfox.org/mozilla-central/rev/cfd1cc461f1efe0d66c2fdc17c024a203d5a2fd8/toolkit/content/widgets/videocontrols.js#1687-1810.
   * There are future plans to eventually combine the two implementations.
   */
  keyDown({ altKey, shiftKey, metaKey, ctrlKey, keyCode }) {
    let video = this.getWeakVideo();
    if (!video) {
      return;
    }

    var keystroke = "";
    if (altKey) {
      keystroke += "alt-";
    }
    if (shiftKey) {
      keystroke += "shift-";
    }
    if (this.contentWindow.navigator.platform.startsWith("Mac")) {
      if (metaKey) {
        keystroke += "accel-";
      }
      if (ctrlKey) {
        keystroke += "control-";
      }
    } else {
      if (metaKey) {
        keystroke += "meta-";
      }
      if (ctrlKey) {
        keystroke += "accel-";
      }
    }

    switch (keyCode) {
      case this.contentWindow.KeyEvent.DOM_VK_UP:
        keystroke += "upArrow";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_DOWN:
        keystroke += "downArrow";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_LEFT:
        keystroke += "leftArrow";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_RIGHT:
        keystroke += "rightArrow";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_HOME:
        keystroke += "home";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_END:
        keystroke += "end";
        break;
      case this.contentWindow.KeyEvent.DOM_VK_SPACE:
        keystroke += "space";
        break;
    }

    const isVideoStreaming = video.duration == +Infinity;
    var oldval, newval;

    try {
      switch (keystroke) {
        case "space" /* Toggle Play / Pause */:
          if (video.paused || video.ended) {
            video.play();
          } else {
            video.pause();
          }
          break;
        case "downArrow" /* Volume decrease */:
          oldval = video.volume;
          video.volume = oldval < 0.1 ? 0 : oldval - 0.1;
          video.muted = false;
          break;
        case "upArrow" /* Volume increase */:
          oldval = video.volume;
          video.volume = oldval > 0.9 ? 1 : oldval + 0.1;
          video.muted = false;
          break;
        case "accel-downArrow" /* Mute */:
          video.muted = true;
          break;
        case "accel-upArrow" /* Unmute */:
          video.muted = false;
          break;
        case "leftArrow": /* Seek back 15 seconds */
        case "accel-leftArrow" /* Seek back 10% */:
          if (isVideoStreaming) {
            return;
          }

          oldval = video.currentTime;
          if (keystroke == "leftArrow") {
            newval = oldval - 15;
          } else {
            newval = oldval - video.duration / 10;
          }
          video.currentTime = newval >= 0 ? newval : 0;
          break;
        case "rightArrow": /* Seek forward 15 seconds */
        case "accel-rightArrow" /* Seek forward 10% */:
          if (isVideoStreaming) {
            return;
          }

          oldval = video.currentTime;
          var maxtime = video.duration;
          if (keystroke == "rightArrow") {
            newval = oldval + 15;
          } else {
            newval = oldval + maxtime / 10;
          }
          video.currentTime = newval <= maxtime ? newval : maxtime;
          break;
        case "home" /* Seek to beginning */:
          if (!isVideoStreaming) {
            video.currentTime = 0;
          }
          break;
        case "end" /* Seek to end */:
          if (!isVideoStreaming && video.currentTime != video.duration) {
            video.currentTime = video.duration;
          }
          break;
        default:
      }
    } catch (e) {
      /* ignore any exception from setting video.currentTime */
    }
  }

  /**
   * The keyboard was used to attempt to open Picture-in-Picture. In this case,
   * find the focused window, and open Picture-in-Picture for the first
   * available video. We suspect this heuristic will handle most cases, though
   * we might refine this later on.
   */
  keyToggle() {
    let focusedWindow = Services.focus.focusedWindow;
    if (focusedWindow) {
      let doc = focusedWindow.document;
      if (doc) {
        let listOfVideos = doc.querySelectorAll("video");
        let video =
          Array.from(listOfVideos).filter(v => !v.paused)[0] || listOfVideos[0];
        if (video) {
          this.togglePictureInPicture(video);
        }
      }
    }
  }
}
