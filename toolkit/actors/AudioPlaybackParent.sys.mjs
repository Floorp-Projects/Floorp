/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class AudioPlaybackParent extends JSWindowActorParent {
  constructor() {
    super();
    this._hasAudioPlayback = false;
    this._hasBlockMedia = false;
  }
  receiveMessage(aMessage) {
    const browser = this.browsingContext.top.embedderElement;
    switch (aMessage.name) {
      case "AudioPlayback:Start":
        this._hasAudioPlayback = true;
        browser.audioPlaybackStarted();
        break;
      case "AudioPlayback:Stop":
        this._hasAudioPlayback = false;
        browser.audioPlaybackStopped();
        break;
      case "AudioPlayback:ActiveMediaBlockStart":
        this._hasBlockMedia = true;
        browser.activeMediaBlockStarted();
        break;
      case "AudioPlayback:ActiveMediaBlockStop":
        this._hasBlockMedia = false;
        browser.activeMediaBlockStopped();
        break;
    }
  }
  didDestroy() {
    const browser = this.browsingContext.top.embedderElement;
    if (browser && this._hasAudioPlayback) {
      browser.audioPlaybackStopped();
    }
    if (browser && this._hasBlockMedia) {
      browser.activeMediaBlockStopped();
    }
  }
}
