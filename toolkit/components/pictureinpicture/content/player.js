/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { PictureInPicture } = ChromeUtils.import(
  "resource://gre/modules/PictureInPicture.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);

// Time to fade the Picture-in-Picture video controls after first opening.
const CONTROLS_FADE_TIMEOUT_MS = 3000;
const RESIZE_DEBOUNCE_RATE_MS = 500;

async function setupPlayer(id, originatingBrowser, videoData) {
  let holder = document.querySelector(".player-holder");
  let browser = document.getElementById("browser");
  browser.remove();

  browser.setAttribute("nodefaultsrc", "true");
  browser.sameProcessAsFrameLoader = originatingBrowser.frameLoader;
  holder.appendChild(browser);

  browser.loadURI("about:blank", {
    triggeringPrincipal: originatingBrowser.contentPrincipal,
  });

  let mm = browser.frameLoader.messageManager;
  mm.sendAsyncMessage("PictureInPicture:SetupPlayer");

  document.getElementById("play").addEventListener("click", () => {
    mm.sendAsyncMessage("PictureInPicture:Play");
  });

  document.getElementById("pause").addEventListener("click", () => {
    mm.sendAsyncMessage("PictureInPicture:Pause");
  });

  document.getElementById("unpip").addEventListener("click", () => {
    PictureInPicture.focusTabAndClosePip();
  });

  // If the content process hosting the video crashes, let's
  // just close the window for now.
  browser.addEventListener("oop-browser-crashed", () => {
    PictureInPicture.closePipWindow({ reason: "browser-crash" });
  });

  let close = document.getElementById("close");
  close.addEventListener("click", () => {
    PictureInPicture.closePipWindow({ reason: "close-button" });
  });

  document.getElementById("controls").setAttribute("showing", true);
  setTimeout(() => {
    document.getElementById("controls").removeAttribute("showing");
  }, CONTROLS_FADE_TIMEOUT_MS);

  Services.telemetry.setEventRecordingEnabled("pictureinpicture", true);

  let resizeDebouncer = new DeferredTask(() => {
    Services.telemetry.recordEvent("pictureinpicture", "resize", "player", id, {
      width: window.outerWidth.toString(),
      height: window.outerHeight.toString(),
    });
  }, RESIZE_DEBOUNCE_RATE_MS);

  addEventListener("resize", e => {
    resizeDebouncer.disarm();
    resizeDebouncer.arm();
  });

  let lastScreenX = window.screenX;
  let lastScreenY = window.screenY;

  addEventListener("mouseout", e => {
    if (window.screenX != lastScreenX || window.screenY != lastScreenY) {
      Services.telemetry.recordEvent("pictureinpicture", "move", "player", id, {
        screenX: window.screenX.toString(),
        screenY: window.screenY.toString(),
      });
    }

    lastScreenX = window.screenX;
    lastScreenY = window.screenY;
  });

  Services.telemetry.recordEvent("pictureinpicture", "create", "player", id, {
    width: window.outerWidth.toString(),
    height: window.outerHeight.toString(),
    screenX: window.screenX.toString(),
    screenY: window.screenY.toString(),
  });

  window.addEventListener("unload", () => {
    resizeDebouncer.disarm();
    PictureInPicture.unload(window);
  });
}
