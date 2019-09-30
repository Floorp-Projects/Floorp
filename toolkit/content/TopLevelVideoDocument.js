/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Hide our variables from the web content, even though the spec allows them
// (and the DOM) to be accessible (see bug 1474832)
{
  // <video> is used for top-level audio documents as well
  let videoElement = document.getElementsByTagName("video")[0];

  let setFocusToVideoElement = function(e) {
    // We don't want to retarget focus if it goes to the controls in
    // the video element. Because they're anonymous content, the target
    // will be the video element in that case. Avoid calling .focus()
    // for those events:
    if (e && e.target == videoElement) {
      return;
    }
    videoElement.focus();
  };

  // Redirect focus to the video element whenever the document receives
  // focus.
  document.addEventListener("focus", setFocusToVideoElement, true);

  // Focus on the video in the newly created document.
  setFocusToVideoElement();

  // Opt out of moving focus away if the DOM tree changes (from add-on or web content)
  let observer = new MutationObserver(() => {
    observer.disconnect();
    document.removeEventListener("focus", setFocusToVideoElement, true);
  });
  observer.observe(document.documentElement, {
    childList: true,
    subtree: true,
  });

  // Handle fullscreen mode
  document.addEventListener("keypress", ev => {
    // Maximize the standalone video when pressing F11,
    // but ignore audio elements
    if (
      ev.key == "F11" &&
      videoElement.videoWidth != 0 &&
      videoElement.videoHeight != 0
    ) {
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

      if (!document.fullscreenElement) {
        videoElement.requestFullscreen();
      } else {
        document.exitFullscreen();
      }
    }
  });
}
