/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function closeFullScreen() {
  sendAsyncMessage("Browser:FullScreenVideo:Close");
}

function startPlayback(aEvent) {
  let target = aEvent.originalTarget;
  Content._sendMouseEvent("mousemove", target, 0, 0);
  Content._sendMouseEvent("mousedown", target, 0, 0);
  Content._sendMouseEvent("mouseup", target, 0, 0);
}

addEventListener("click", function(aEvent) {
  if (aEvent.target.id == "close")
    closeFullScreen();
}, false);

addEventListener("CloseVideo", closeFullScreen, false);
addEventListener("StartVideo", startPlayback, false);

addEventListener("PlayVideo", function() {
  sendAsyncMessage("Browser:FullScreenVideo:Play");
}, false);

addEventListener("PauseVideo", function() {
  sendAsyncMessage("Browser:FullScreenVideo:Pause");
}, false);

