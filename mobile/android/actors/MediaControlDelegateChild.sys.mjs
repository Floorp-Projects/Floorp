/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MediaUtils: "resource://gre/modules/MediaUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

export class MediaControlDelegateChild extends GeckoViewActorChild {
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        this.handleFullscreenChanged(true);
        break;
    }
  }

  async handleFullscreenChanged(retry) {
    debug`handleFullscreenChanged`;

    const element = this.document.fullscreenElement;
    const mediaElement = lazy.MediaUtils.findMediaElement(element);

    if (element && !mediaElement) {
      // Non-media element fullscreen.
      debug`No fullscreen media element found.`;
    }

    const activated = await this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:MediaSession:Fullscreen",
      metadata: lazy.MediaUtils.getMetadata(mediaElement) ?? {},
      enabled: !!element,
    });
    if (activated) {
      return;
    }
    if (retry && element) {
      // When media session is going to active, we have a race condition of
      // full screen event because media session will be activated by full
      // screen event.
      // So we retry to call media session delegate for this situation.
      lazy.setTimeout(() => {
        this.handleFullscreenChanged(false);
      }, 100);
    }
  }
}

const { debug } = MediaControlDelegateChild.initLogging(
  "MediaControlDelegateChild"
);
