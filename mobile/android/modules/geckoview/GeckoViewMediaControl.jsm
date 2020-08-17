/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewMediaControl"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);

class GeckoViewMediaControl extends GeckoViewModule {
  onInit() {
    debug`onInit`;
  }

  onEnable() {
    debug`onEnable`;

    if (this.controller.isActive) {
      this.handleActivated();
    }

    const options = {
      mozSystemGroup: true,
      capture: false,
    };

    this.controller.addEventListener("activated", this, options);
    this.controller.addEventListener("deactivated", this, options);
    this.controller.addEventListener("supportedkeyschange", this, options);
    this.controller.addEventListener("positionstatechange", this, options);
    // TODO: Move other events to webidl once supported.
  }

  onDisable() {
    debug`onDisable`;

    this.controller.removeEventListener("activated", this);
    this.controller.removeEventListener("deactivated", this);
    this.controller.removeEventListener("supportedkeyschange", this);
    this.controller.removeEventListener("positionstatechange", this);
  }

  get controller() {
    return this.browser.browsingContext.mediaController;
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
      default:
        warn`Unknown event type ${aEvent.type}`;
        break;
    }
  }

  handleActivated() {
    debug`handleActivated`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Activated",
      id: this.controller.id,
    });
  }

  handleDeactivated() {
    debug`handleDeactivated`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Deactivated",
      id: this.controller.id,
    });
  }

  handlePositionStateChanged(aEvent) {
    debug`handlePositionStateChanged`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:PositionState",
      id: this.controller.id,
      state: {
        duration: aEvent.duration,
        playbackRate: aEvent.playbackRate,
        position: aEvent.position,
      }
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
      id: this.controller.id,
      features,
    });
  }
}

const { debug, warn } = GeckoViewMediaControl.initLogging(
  "GeckoViewMediaControl"
);
