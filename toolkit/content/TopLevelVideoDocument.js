/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// <video> is used for top-level audio documents as well
let videoElement = document.getElementsByTagName("video")[0];

// Redirect focus to the video element whenever the document receives
// focus.
document.addEventListener("focus", () => videoElement.focus(), true);

// Handle fullscreen mode
document.addEventListener("keypress", ev => {
  // Maximize the standalone video when pressing F11,
  // but ignore audio elements
  if (ev.key == "F11" && videoElement.videoWidth != 0 && videoElement.videoHeight != 0) {
    // If we're in browser fullscreen mode, it means the user pressed F11
    // while browser chrome or another tab had focus.
    // Don't break leaving that mode, so do nothing here.
    if (window.fullScreen) {
      return;
    }

    // If we're not in browser fullscreen mode, prevent entering into that,
    // so we don't end up there after pressing Esc.
    ev.preventDefault();
    ev.stopPropagation();

    if (!document.mozFullScreenElement) {
      videoElement.mozRequestFullScreen();
    } else {
      document.mozCancelFullScreen();
    }
  }
});
