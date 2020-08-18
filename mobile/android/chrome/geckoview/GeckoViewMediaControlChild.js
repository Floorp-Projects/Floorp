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

XPCOMUtils.defineLazyModuleGetters(this, {
  MediaUtils: "resource://gre/modules/MediaUtils.jsm",
});

class GeckoViewMediaControl extends GeckoViewChildModule {
  onInit() {
    debug`onEnable`;
  }

  onEnable() {
    debug`onEnable`;

    addEventListener("MozDOMFullscreen:Entered", this, false);
    addEventListener("MozDOMFullscreen:Exited", this, false);
  }

  onDisable() {
    debug`onDisable`;

    removeEventListener("MozDOMFullscreen:Entered", this);
    removeEventListener("MozDOMFullscreen:Exited", this);
  }

  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        this.handleFullscreenChanged();
        break;
    }
  }

  handleFullscreenChanged() {
    const element = content && content.document.fullscreenElement;
    const mediaElement = MediaUtils.findMediaElement(element);

    if (element && !mediaElement) {
      // Non-media element fullscreen.
      return;
    }

    const message = {
      metadata: MediaUtils.getMetadata(mediaElement) ?? {},
      enabled: !!element,
    };

    this.messageManager.sendAsyncMessage(
      "GeckoView:MediaControl:Fullscreen",
      message
    );
  }
}

const { debug } = GeckoViewMediaControl.initLogging("GeckoViewMediaControl");
const module = GeckoViewMediaControl.create(this);
