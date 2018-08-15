/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AudioPlaybackChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

class AudioPlaybackChild extends ActorChild {
  handleMediaControlMessage(msg) {
    let utils = this.content.windowUtils;
    let suspendTypes = Ci.nsISuspendedTypes;
    switch (msg) {
      case "mute":
        utils.audioMuted = true;
        break;
      case "unmute":
        utils.audioMuted = false;
        break;
      case "lostAudioFocus":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "lostAudioFocusTransiently":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE;
        break;
      case "gainAudioFocus":
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      case "mediaControlPaused":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "mediaControlStopped":
        utils.mediaSuspend = suspendTypes.SUSPENDED_STOP_DISPOSABLE;
        break;
      case "resumeMedia":
        // User has clicked the tab audio indicator to play a delayed
        // media. That's clear user intent to play, so gesture activate
        // the content document tree so that the block-autoplay logic
        // allows the media to autoplay.
        this.content.document.notifyUserGestureActivation();
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      default:
        dump("Error : wrong media control msg!\n");
        break;
    }
  }

  observe(subject, topic, data) {
    if (topic === "audio-playback") {
      if (subject && subject.top == this.content) {
        let name = "AudioPlayback:";
        if (data === "activeMediaBlockStart") {
          name += "ActiveMediaBlockStart";
        } else if (data === "activeMediaBlockStop") {
          name += "ActiveMediaBlockStop";
        } else {
          name += (data === "active") ? "Start" : "Stop";
        }
        this.mm.sendAsyncMessage(name);
      }
    }
  }

  receiveMessage(msg) {
    if (msg.name == "AudioPlayback") {
      this.handleMediaControlMessage(msg.data.type);
    }
  }
}
