/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  MediaUtils: "resource://gre/modules/MediaUtils.jsm",
});

var EXPORTED_SYMBOLS = ["MediaControlDelegateChild"];

class MediaControlDelegateChild extends GeckoViewActorChild {
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
    debug`handleFullscreenChanged`;

    const element = this.document.fullscreenElement;
    const mediaElement = lazy.MediaUtils.findMediaElement(element);

    if (element && !mediaElement) {
      // Non-media element fullscreen.
      debug`No fullscreen media element found.`;
    }

    this.eventDispatcher.sendRequest({
      type: "GeckoView:MediaSession:Fullscreen",
      metadata: lazy.MediaUtils.getMetadata(mediaElement) ?? {},
      enabled: !!element,
    });
  }
}

const { debug } = MediaControlDelegateChild.initLogging(
  "MediaControlDelegateChild"
);
