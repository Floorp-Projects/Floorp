/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PictureInPictureChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

// A weak reference to the most recent <video> in this content
// process that is being viewed in Picture-in-Picture.
var gWeakVideo = null;
// A weak reference to the content window of the most recent
// Picture-in-Picture window for this content process.
var gWeakPlayerContent = null;

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
