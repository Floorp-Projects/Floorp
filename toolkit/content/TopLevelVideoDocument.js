/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

window.addEventListener("load", () => {
  // <video> is used for top-level audio documents as well
  let videoElement = document.getElementsByTagName("video")[0];
  if (!videoElement)
    return;

  // Send keystrokes to the video element when the body element is focused,
  // to be received by the event listener in videocontrols.xml.
  document.addEventListener("keypress", ev => {
    if (ev.synthetic) // prevent recursion
      return;

    // Check if the video element is focused, so it already receives
    // keystrokes, and don't send it another one from here.
    if (document.activeElement == videoElement)
      return;

    let newEvent = new KeyboardEvent("keypress", ev);
    newEvent.synthetic = true;
    videoElement.dispatchEvent(newEvent);
  });
});
