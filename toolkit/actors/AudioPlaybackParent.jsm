/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AudioPlaybackParent"];

class AudioPlaybackParent extends JSWindowActorParent {
  constructor() {
    super();
    this._hasAudioPlayback = false;
    this._hasBlockMedia = false;
    this._topLevelBrowser = null;
  }
  receiveMessage(aMessage) {
    // To ensure we can still access browser element in `didDestroy()` because
    // we might not be able to access that from browsing context at that time.
    this._topLevelBrowser = this.browsingContext.top.embedderElement;
    switch (aMessage.name) {
      case "AudioPlayback:Start":
        this._hasAudioPlayback = true;
        this._topLevelBrowser.audioPlaybackStarted();
        break;
      case "AudioPlayback:Stop":
        this._hasAudioPlayback = false;
        this._topLevelBrowser.audioPlaybackStopped();
        break;
      case "AudioPlayback:ActiveMediaBlockStart":
        this._hasBlockMedia = true;
        this._topLevelBrowser.activeMediaBlockStarted();
        break;
      case "AudioPlayback:ActiveMediaBlockStop":
        this._hasBlockMedia = false;
        this._topLevelBrowser.activeMediaBlockStopped();
        break;
    }
  }
  didDestroy() {
    if (this._hasAudioPlayback) {
      this._topLevelBrowser.audioPlaybackStopped();
    }
    if (this._hasBlockMedia) {
      this._topLevelBrowser.activeMediaBlockStopped();
    }
    this._topLevelBrowser = null;
  }
}
