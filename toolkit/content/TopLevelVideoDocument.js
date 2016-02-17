/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// <video> is used for top-level audio documents as well
let videoElement = document.getElementsByTagName("video")[0];

// 1. Handle fullscreen mode;
// 2. Send keystrokes to the video element if the body element is focused,
//    to be received by the event listener in videocontrols.xml.
document.addEventListener("keypress", ev => {
  if (ev.synthetic) // prevent recursion
    return;

  // Maximize the standalone video when pressing F11,
  // but ignore audio elements
  if (ev.key == "F11" && videoElement.videoWidth != 0 && videoElement.videoHeight != 0) {
    // If we're in browser fullscreen mode, it means the user pressed F11
    // while browser chrome or another tab had focus.
    // Don't break leaving that mode, so do nothing here.
    if (window.fullScreen) {
      return;
    }

    // If we're not in broser fullscreen mode, prevent entering into that,
    // so we don't end up there after pressing Esc.
    ev.preventDefault();
    ev.stopPropagation();

    if (!document.fullscreenElement) {
      videoElement.requestFullscreen();
    } else {
      document.exitFullscreen();
    }
    return;
  }

  // Check if the video element is focused, so it already receives
  // keystrokes, and don't send it another one from here.
  if (document.activeElement == videoElement)
    return;

  let newEvent = new KeyboardEvent("keypress", ev);
  newEvent.synthetic = true;
  videoElement.dispatchEvent(newEvent);
});
