/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PictureInPictureChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

var gWeakVideo = null;

class PictureInPictureChild extends ActorChild {
  handleEvent(event) {
    switch (event.type) {
      case "MozTogglePictureInPicture": {
        this.togglePictureInPicture(event.target);
        break;
      }
    }
  }

  togglePictureInPicture(video) {
    if (this.inPictureInPicture(video)) {
      this.closePictureInPicture(video);
    } else {
      this.requestPictureInPicture(video);
    }
  }

  inPictureInPicture(video) {
    return gWeakVideo && gWeakVideo.get() === video;
  }

  closePictureInPicture() {
    this.mm.sendAsyncMessage("PictureInPicture:Close", {
      browingContextId: this.docShell.browsingContext.id,
    });
  }

  requestPictureInPicture(video) {
    gWeakVideo = Cu.getWeakReference(video);
    this.mm.sendAsyncMessage("PictureInPicture:Request", {
      videoHeight: video.videoHeight,
      videoWidth: video.videoWidth,
    });
  }

  receiveMessage(message) {
    switch (message.name) {
      case "PictureInPicture:SetupPlayer": {
        this.setupPlayer();
        break;
      }
    }
  }

  async setupPlayer() {
    if (!gWeakVideo) {
      this.closePictureInPicture();
    }

    let originatingVideo = gWeakVideo.get();
    if (!originatingVideo) {
      this.closePictureInPicture();
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

    let originatingWindow = originatingVideo.ownerGlobal;
    originatingWindow.addEventListener("unload", (e) => {
      this.closePictureInPicture(originatingVideo);
    }, { once: true });

    this.content.addEventListener("unload", () => {
      let video = gWeakVideo && gWeakVideo.get();
      if (video) {
        video.stopCloningElementVisually();
      }
      gWeakVideo = null;
    }, { once: true });
  }
}
