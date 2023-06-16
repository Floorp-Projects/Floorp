/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

export class GeckoViewMediaControl extends GeckoViewModule {
  onInit() {
    debug`onInit`;
  }

  onInitBrowser() {
    debug`onInitBrowser`;

    const options = {
      mozSystemGroup: true,
      capture: false,
    };

    this.controller.addEventListener("activated", this, options);
    this.controller.addEventListener("deactivated", this, options);
    this.controller.addEventListener("supportedkeyschange", this, options);
    this.controller.addEventListener("positionstatechange", this, options);
    this.controller.addEventListener("metadatachange", this, options);
    this.controller.addEventListener("playbackstatechange", this, options);
  }

  onDestroyBrowser() {
    debug`onDestroyBrowser`;

    this.controller.removeEventListener("activated", this);
    this.controller.removeEventListener("deactivated", this);
    this.controller.removeEventListener("supportedkeyschange", this);
    this.controller.removeEventListener("positionstatechange", this);
    this.controller.removeEventListener("metadatachange", this);
    this.controller.removeEventListener("playbackstatechange", this);
  }

  onEnable() {
    debug`onEnable`;

    if (this.controller.isActive) {
      this.handleActivated();
    }

    this.registerListener([
      "GeckoView:MediaSession:Play",
      "GeckoView:MediaSession:Pause",
      "GeckoView:MediaSession:Stop",
      "GeckoView:MediaSession:NextTrack",
      "GeckoView:MediaSession:PrevTrack",
      "GeckoView:MediaSession:SeekForward",
      "GeckoView:MediaSession:SeekBackward",
      "GeckoView:MediaSession:SkipAd",
      "GeckoView:MediaSession:SeekTo",
      "GeckoView:MediaSession:MuteAudio",
    ]);
  }

  onDisable() {
    debug`onDisable`;

    this.unregisterListener();
  }

  get controller() {
    return this.browser.browsingContext.mediaController;
  }

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:MediaSession:Play":
        this.controller.play();
        break;
      case "GeckoView:MediaSession:Pause":
        this.controller.pause();
        break;
      case "GeckoView:MediaSession:Stop":
        this.controller.stop();
        break;
      case "GeckoView:MediaSession:NextTrack":
        this.controller.nextTrack();
        break;
      case "GeckoView:MediaSession:PrevTrack":
        this.controller.prevTrack();
        break;
      case "GeckoView:MediaSession:SeekForward":
        this.controller.seekForward();
        break;
      case "GeckoView:MediaSession:SeekBackward":
        this.controller.seekBackward();
        break;
      case "GeckoView:MediaSession:SkipAd":
        this.controller.skipAd();
        break;
      case "GeckoView:MediaSession:SeekTo":
        this.controller.seekTo(aData.time, aData.fast);
        break;
      case "GeckoView:MediaSession:MuteAudio":
        if (aData.mute) {
          this.browser.mute();
        } else {
          this.browser.unmute();
        }
        break;
    }
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "activated":
        this.handleActivated();
        break;
      case "deactivated":
        this.handleDeactivated();
        break;
      case "supportedkeyschange":
        this.handleSupportedKeysChanged();
        break;
      case "positionstatechange":
        this.handlePositionStateChanged(aEvent);
        break;
      case "metadatachange":
        this.handleMetadataChanged();
        break;
      case "playbackstatechange":
        this.handlePlaybackStateChanged();
        break;
      default:
        warn`Unknown event type ${aEvent.type}`;
        break;
    }
  }

  handleActivated() {
    debug`handleActivated`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Activated",
    });
  }

  handleDeactivated() {
    debug`handleDeactivated`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Deactivated",
    });
  }

  handlePositionStateChanged(aEvent) {
    debug`handlePositionStateChanged`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:PositionState",
      state: {
        duration: aEvent.duration,
        playbackRate: aEvent.playbackRate,
        position: aEvent.position,
      },
    });
  }

  handleSupportedKeysChanged() {
    const supported = this.controller.supportedKeys;

    debug`handleSupportedKeysChanged ${supported}`;

    // Mapping it to a key-value store for compatibility with the JNI
    // implementation for now.
    const features = new Map();
    supported.forEach(key => {
      features[key] = true;
    });

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Features",
      features,
    });
  }

  handleMetadataChanged() {
    let metadata = null;
    try {
      metadata = this.controller.getMetadata();
    } catch (e) {
      warn`Metadata not available`;
    }
    debug`handleMetadataChanged ${metadata}`;

    if (metadata) {
      this.eventDispatcher.sendRequest({
        type: "GeckoView:MediaSession:Metadata",
        metadata,
      });
    }
  }

  handlePlaybackStateChanged() {
    const state = this.controller.playbackState;
    let type = null;

    debug`handlePlaybackStateChanged ${state}`;

    switch (state) {
      case "none":
        type = "GeckoView:MediaSession:Playback:None";
        break;
      case "paused":
        type = "GeckoView:MediaSession:Playback:Paused";
        break;
      case "playing":
        type = "GeckoView:MediaSession:Playback:Playing";
        break;
    }

    if (!type) {
      return;
    }

    this.eventDispatcher.sendRequest({
      type,
    });
  }
}

const { debug, warn } = GeckoViewMediaControl.initLogging(
  "GeckoViewMediaControl"
);
