/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { GeckoViewChildModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewChildModule.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

class GeckoViewMediaChild extends GeckoViewChildModule {
  onInit() {
    this._videoIndex = 0;
    XPCOMUtils.defineLazyGetter(this, "_videos", () => new Map());
    this._mediaEvents = [
      "abort",
      "canplay",
      "canplaythrough",
      "durationchange",
      "emptied",
      "ended",
      "error",
      "loadeddata",
      "loadedmetadata",
      "pause",
      "play",
      "playing",
      "progress",
      "ratechange",
      "resize",
      "seeked",
      "seeking",
      "stalled",
      "suspend",
      "timeupdate",
      "volumechange",
      "waiting",
    ];

    this._mediaEventCallback = event => {
      this.handleMediaEvent(event);
    };
    this._fullscreenMedia = null;
    this._stateSymbol = Symbol();
  }

  onEnable() {
    debug`onEnable`;
    addEventListener("UAWidgetSetupOrChange", this, false);
    addEventListener("MozDOMFullscreen:Entered", this, false);
    addEventListener("MozDOMFullscreen:Exited", this, false);
    addEventListener("pagehide", this, false);

    this.messageManager.addMessageListener("GeckoView:MediaObserve", this);
    this.messageManager.addMessageListener("GeckoView:MediaUnobserve", this);
    this.messageManager.addMessageListener("GeckoView:MediaPlay", this);
    this.messageManager.addMessageListener("GeckoView:MediaPause", this);
    this.messageManager.addMessageListener("GeckoView:MediaSeek", this);
    this.messageManager.addMessageListener("GeckoView:MediaSetVolume", this);
    this.messageManager.addMessageListener("GeckoView:MediaSetMuted", this);
    this.messageManager.addMessageListener(
      "GeckoView:MediaSetPlaybackRate",
      this
    );
  }

  onDisable() {
    debug`onDisable`;

    removeEventListener("UAWidgetSetupOrChange", this);
    removeEventListener("MozDOMFullscreen:Entered", this);
    removeEventListener("MozDOMFullscreen:Exited", this);
    removeEventListener("pagehide", this);

    this.messageManager.removeMessageListener("GeckoView:MediaObserve", this);
    this.messageManager.removeMessageListener("GeckoView:MediaUnobserve", this);
    this.messageManager.removeMessageListener("GeckoView:MediaPlay", this);
    this.messageManager.removeMessageListener("GeckoView:MediaPause", this);
    this.messageManager.removeMessageListener("GeckoView:MediaSeek", this);
    this.messageManager.removeMessageListener("GeckoView:MediaSetVolume", this);
    this.messageManager.removeMessageListener("GeckoView:MediaSetMuted", this);
    this.messageManager.removeMessageListener(
      "GeckoView:MediaSetPlaybackRate",
      this
    );
  }

  receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    const data = aMsg.data;
    const element = this.findElement(data.id);
    if (!element) {
      warn`Didn't find HTMLMediaElement with id: ${data.id}`;
      return;
    }

    switch (aMsg.name) {
      case "GeckoView:MediaObserve":
        this.observeMedia(element);
        break;
      case "GeckoView:MediaUnobserve":
        this.unobserveMedia(element);
        break;
      case "GeckoView:MediaPlay":
        element.play();
        break;
      case "GeckoView:MediaPause":
        element.pause();
        break;
      case "GeckoView:MediaSeek":
        element.currentTime = data.time;
        break;
      case "GeckoView:MediaSetVolume":
        element.volume = data.volume;
        break;
      case "GeckoView:MediaSetMuted":
        element.muted = !!data.muted;
        break;
      case "GeckoView:MediaSetPlaybackRate":
        element.playbackRate = data.playbackRate;
        break;
    }
  }

  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "UAWidgetSetupOrChange":
        this.handleNewMedia(aEvent.composedTarget);
        break;
      case "MozDOMFullscreen:Entered":
        const element = content && content.document.fullscreenElement;
        if (this.isMedia(element)) {
          this.handleFullscreenChange(element);
        } else {
          // document.fullscreenElement can be a div container instead of the HTMLMediaElement
          // in some pages (e.g Vimeo).
          const childrenMedias = element.getElementsByTagName("video");
          if (childrenMedias && childrenMedias.length > 0) {
            this.handleFullscreenChange(childrenMedias[0]);
          }
        }
        break;
      case "MozDOMFullscreen:Exited":
        this.handleFullscreenChange(null);
        break;
      case "pagehide":
        if (aEvent.target === content.document) {
          this.handleWindowUnload();
        }
        break;
    }
  }

  handleWindowUnload() {
    for (const weakElement of this._videos.values()) {
      const element = weakElement.get();
      if (element) {
        this.unobserveMedia(element);
      }
    }
    if (this._videos.size > 0) {
      this.notifyMediaRemoveAll();
    }
    this._videos.clear();
    this._fullscreenMedia = null;
  }

  handleNewMedia(aElement) {
    if (this.getState(aElement) || !this.isMedia(aElement)) {
      return;
    }
    const state = {
      id: ++this._videoIndex,
      notified: false,
      observing: false,
    };
    aElement[this._stateSymbol] = state;
    this._videos.set(state.id, Cu.getWeakReference(aElement));

    this.notifyNewMedia(aElement);
  }

  notifyNewMedia(aElement) {
    this.getState(aElement).notified = true;
    const message = {
      type: "GeckoView:MediaAdd",
      id: this.getState(aElement).id,
    };

    this.fillMetadata(aElement, message);
    this.eventDispatcher.sendRequest(message);
  }

  observeMedia(aElement) {
    if (this.isObserving(aElement)) {
      return;
    }
    this.getState(aElement).observing = true;

    for (const name of this._mediaEvents) {
      aElement.addEventListener(name, this._mediaEventCallback, true);
    }

    // Notify current state
    this.notifyTimeChange(aElement);
    this.notifyVolumeChange(aElement);
    this.notifyRateChange(aElement);
    if (aElement.readyState >= 1) {
      this.notifyMetadataChange(aElement);
    }
    this.notifyReadyStateChange(aElement);
    if (!aElement.paused) {
      this.notifyPlaybackStateChange(aElement, "play");
    }
    if (aElement === this._fullscreenMedia) {
      this.notifyFullscreenChange(aElement, true);
    }
    if (this.hasError(aElement)) {
      this.notifyMediaError(aElement);
    }
  }

  unobserveMedia(aElement) {
    if (!this.isObserving(aElement)) {
      return;
    }
    this.getState(aElement).observing = false;
    for (const name of this._mediaEvents) {
      aElement.removeEventListener(name, this._mediaEventCallback);
    }
  }

  isMedia(aElement) {
    if (!aElement) {
      return false;
    }
    const elementType = ChromeUtils.getClassName(aElement);
    return (
      elementType === "HTMLVideoElement" || elementType === "HTMLAudioElement"
    );
  }

  isObserving(aElement) {
    return (
      aElement && this.getState(aElement) && this.getState(aElement).observing
    );
  }

  findElement(aId) {
    for (const weakElement of this._videos.values()) {
      const element = weakElement.get();
      if (element && this.getState(element).id === aId) {
        return element;
      }
    }
    return null;
  }

  getState(aElement) {
    return aElement[this._stateSymbol];
  }

  hasError(aElement) {
    // We either have an explicit error, or networkState is set to NETWORK_NO_SOURCE
    // after selecting a source.
    return (
      aElement.error != null ||
      (aElement.networkState === aElement.NETWORK_NO_SOURCE &&
        this.hasSources(aElement))
    );
  }

  hasSources(aElement) {
    if (aElement.hasAttribute("src") && aElement.getAttribute("src") !== "") {
      return true;
    }
    for (
      var child = aElement.firstChild;
      child !== null;
      child = child.nextElementSibling
    ) {
      if (child instanceof content.window.HTMLSourceElement) {
        return true;
      }
    }
    return false;
  }

  fillMetadata(aElement, aOut) {
    aOut.src = aElement.currentSrc || aElement.src;
    aOut.width = aElement.videoWidth || 0;
    aOut.height = aElement.videoHeight || 0;
    aOut.duration = aElement.duration;
    aOut.seekable = !!aElement.seekable;
    if (aElement.audioTracks) {
      aOut.audioTrackCount = aElement.audioTracks.length;
    } else {
      aOut.audioTrackCount =
        aElement.mozHasAudio ||
        aElement.webkitAudioDecodedByteCount ||
        ChromeUtils.getClassName(aElement) === "HTMLAudioElement"
          ? 1
          : 0;
    }
    if (aElement.videoTracks) {
      aOut.videoTrackCount = aElement.videoTracks.length;
    } else {
      aOut.videoTrackCount =
        ChromeUtils.getClassName(aElement) === "HTMLVideoElement" ? 1 : 0;
    }
  }

  handleMediaEvent(aEvent) {
    const element = aEvent.target;
    if (!this.isObserving(element)) {
      return;
    }
    switch (aEvent.type) {
      case "timeupdate":
        this.notifyTimeChange(element);
        break;
      case "volumechange":
        this.notifyVolumeChange(element);
        break;
      case "loadedmetadata":
        this.notifyMetadataChange(element);
        this.notifyReadyStateChange(element);
        break;
      case "ratechange":
        this.notifyRateChange(element);
        break;
      case "error":
        this.notifyMediaError(element);
        break;
      case "progress":
        this.notifyLoadProgress(element, aEvent);
        break;
      case "durationchange": // Fallthrough
      case "resize":
        this.notifyMetadataChange(element);
        break;
      case "canplay": // Fallthrough
      case "canplaythrough": // Fallthrough
      case "loadeddata":
        this.notifyReadyStateChange(element);
        break;
      default:
        this.notifyPlaybackStateChange(element, aEvent.type);
        break;
    }
  }

  handleFullscreenChange(aElement) {
    if (aElement === this._fullscreenMedia) {
      return;
    }

    if (this.isObserving(this._fullscreenMedia)) {
      this.notifyFullscreenChange(this._fullscreenMedia, false);
    }
    this._fullscreenMedia = aElement;

    if (this.isObserving(aElement)) {
      this.notifyFullscreenChange(aElement, true);
    }
  }

  notifyPlaybackStateChange(aElement, aName) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaPlaybackStateChanged",
      id: this.getState(aElement).id,
      playbackState: aName,
    });
  }

  notifyReadyStateChange(aElement) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaReadyStateChanged",
      id: this.getState(aElement).id,
      readyState: aElement.readyState,
    });
  }

  notifyMetadataChange(aElement) {
    const message = {
      type: "GeckoView:MediaMetadataChanged",
      id: this.getState(aElement).id,
    };
    this.fillMetadata(aElement, message);
    this.eventDispatcher.sendRequest(message);
  }

  notifyLoadProgress(aElement, aEvent) {
    const message = {
      type: "GeckoView:MediaProgress",
      id: this.getState(aElement).id,
      loadedBytes: aEvent.lengthComputable ? aEvent.loaded : -1,
      totalBytes: aEvent.lengthComputable ? aEvent.total : -1,
    };
    if (aElement.buffered && aElement.buffered.length > 0) {
      message.timeRangeStarts = [];
      message.timeRangeEnds = [];
      for (let i = 0; i < aElement.buffered.length; ++i) {
        message.timeRangeStarts.push(aElement.buffered.start(i));
        message.timeRangeEnds.push(aElement.buffered.end(i));
      }
    }
    this.eventDispatcher.sendRequest(message);
  }

  notifyTimeChange(aElement) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaTimeChanged",
      id: this.getState(aElement).id,
      time: aElement.currentTime,
    });
  }

  notifyRateChange(aElement) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaRateChanged",
      id: this.getState(aElement).id,
      rate: aElement.playbackRate,
    });
  }

  notifyVolumeChange(aElement) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaVolumeChanged",
      id: this.getState(aElement).id,
      volume: aElement.volume,
      muted: !!aElement.muted,
    });
  }

  notifyFullscreenChange(aElement, aIsFullscreen) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaFullscreenChanged",
      id: this.getState(aElement).id,
      fullscreen: aIsFullscreen,
    });
  }

  notifyMediaError(aElement) {
    const code = aElement.error ? aElement.error.code : 0;
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaError",
      id: this.getState(aElement).id,
      code,
    });
  }

  notifyMediaRemove(aElement) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaRemove",
      id: this.getState(aElement).id,
    });
  }

  notifyMediaRemoveAll() {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaRemoveAll",
    });
  }
}

const { debug, warn } = GeckoViewMediaChild.initLogging("GeckoViewMedia"); // eslint-disable-line no-unused-vars
const module = GeckoViewMediaChild.create(this);
