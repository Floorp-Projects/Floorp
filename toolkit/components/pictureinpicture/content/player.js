/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {PictureInPicture} = ChromeUtils.import("resource://gre/modules/PictureInPicture.jsm");

async function setupPlayer(originatingBrowser, videoData) {
  window.windowUtils.setChromeMargin(0, 0, 0, 0);
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
    window.close();
  });

  browser.addEventListener("unload", () => {
    PictureInPicture.unload();
  });

  await window.promiseDocumentFlushed(() => {});
  browser.style.MozWindowDragging = "drag";

  let close = document.getElementById("close");
  close.addEventListener("click", () => {
    window.close();
  });
}
