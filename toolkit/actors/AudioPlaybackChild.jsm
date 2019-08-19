/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AudioPlaybackChild"];

class AudioPlaybackChild extends JSWindowActorChild {
  receiveMessage({ name, data }) {
    switch (name) {
      case "AudioPlayback":
        this.handleMediaControlMessage(data.type);
        break;
    }
  }

  handleMediaControlMessage(msg) {
    let utils = this.contentWindow.windowUtils;
    if (!utils) {
      return;
    }

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
      default:
        dump("Error : wrong media control msg!\n");
        break;
    }
  }

  observe(subject, topic, data) {
    if (topic === "audio-playback") {
      let name = "AudioPlayback:";
      if (data === "activeMediaBlockStart") {
        name += "ActiveMediaBlockStart";
      } else if (data === "activeMediaBlockStop") {
        name += "ActiveMediaBlockStop";
      } else {
        name += data === "active" ? "Start" : "Stop";
      }
      this.sendAsyncMessage(name);
    }
  }
}
