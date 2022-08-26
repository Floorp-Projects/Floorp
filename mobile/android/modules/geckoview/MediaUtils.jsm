/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["MediaUtils"];

const MediaUtils = {
  getMetadata(aElement) {
    if (!aElement) {
      return null;
    }
    return {
      src: aElement.currentSrc ?? aElement.src,
      width: aElement.videoWidth ?? 0,
      height: aElement.videoHeight ?? 0,
      duration: aElement.duration,
      seekable: !!aElement.seekable,
      audioTrackCount:
        aElement.audioTracks?.length ??
        aElement.mozHasAudio ??
        aElement.webkitAudioDecodedByteCount ??
        MediaUtils.isAudioElement(aElement)
          ? 1
          : 0,
      videoTrackCount:
        aElement.videoTracks?.length ?? MediaUtils.isVideoElement(aElement)
          ? 1
          : 0,
    };
  },

  isVideoElement(aElement) {
    return (
      aElement && ChromeUtils.getClassName(aElement) === "HTMLVideoElement"
    );
  },

  isAudioElement(aElement) {
    return (
      aElement && ChromeUtils.getClassName(aElement) === "HTMLAudioElement"
    );
  },

  isMediaElement(aElement) {
    return (
      MediaUtils.isVideoElement(aElement) || MediaUtils.isAudioElement(aElement)
    );
  },

  findMediaElement(aElement) {
    return (
      MediaUtils.findVideoElement(aElement) ??
      MediaUtils.findAudioElement(aElement)
    );
  },

  findVideoElement(aElement) {
    if (!aElement) {
      return null;
    }
    if (MediaUtils.isVideoElement(aElement)) {
      return aElement;
    }
    const childrenMedia = aElement.getElementsByTagName("video");
    if (childrenMedia && childrenMedia.length) {
      return childrenMedia[0];
    }
    return null;
  },

  findAudioElement(aElement) {
    if (!aElement || MediaUtils.isAudioElement(aElement)) {
      return aElement;
    }
    const childrenMedia = aElement.getElementsByTagName("audio");
    if (childrenMedia && childrenMedia.length) {
      return childrenMedia[0];
    }
    return null;
  },
};
