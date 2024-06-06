/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * Spotify embeds default to "track preview mode". They require first-party
 * storage access in order to detect the login status and allow the user to play
 * the whole song or add it to their library.
 * Upon clicking the "play" button in the preview view this shim attempts to get
 * storage access and on success, reloads the frame and plays the full track.
 * This only works if the user is already logged in to Spotify in the
 * first-party context.
 */

const AUTOPLAY_FLAG = "shimPlayAfterStorageAccess";
const SELECTOR_PREVIEW_PLAY = 'div[data-testid="preview-play-pause"] > button';
const SELECTOR_FULL_PLAY = 'button[data-testid="play-pause-button"]';

/**
 * Promise-wrapper around DOMContentLoaded event.
 */
function waitForDOMContentLoaded() {
  return new Promise(resolve => {
    window.addEventListener("DOMContentLoaded", resolve, { once: true });
  });
}

/**
 * Listener for the preview playback button which requests storage access and
 * reloads the page.
 */
function previewPlayButtonListener(event) {
  const { target, isTrusted } = event;
  if (!isTrusted) {
    return;
  }

  const button = target.closest("button");
  if (!button) {
    return;
  }

  // Filter for the preview playback button. This won't match the full
  // playback button that is shown when the user is logged in.
  if (!button.matches(SELECTOR_PREVIEW_PLAY)) {
    return;
  }

  // The storage access request below runs async so playback won't start
  // immediately. Mitigate this UX issue by updating the clicked element's
  // style so the user gets some immediate feedback.
  button.style.opacity = 0.5;
  event.stopPropagation();
  event.preventDefault();

  console.debug("Requesting storage access.", location.origin);
  document
    .requestStorageAccess()
    // When storage access is granted, reload the frame for the embedded
    // player to detect the login state and give us full playback
    // capabilities.
    .then(() => {
      // Use a flag to indicate that we want to click play after reload.
      // This is so the user does not have to click play twice.
      sessionStorage.setItem(AUTOPLAY_FLAG, "true");
      console.debug("Reloading after storage access grant.");
      location.reload();
    })
    // If the user denies the storage access prompt we can't use the login
    // state. Attempt start preview playback instead.
    .catch(() => {
      button.click();
    })
    // Reset button style for both success and error case.
    .finally(() => {
      button.style.opacity = 1.0;
    });
}

/**
 * Attempt to start (full) playback. Waits for the play button to appear and
 * become ready.
 */
async function startFullPlayback() {
  await document.requestStorageAccess();

  // Wait for DOMContentLoaded before looking for the playback button.
  await waitForDOMContentLoaded();

  let numTries = 0;
  let intervalId = setInterval(() => {
    try {
      document.querySelector(SELECTOR_FULL_PLAY).click();
      clearInterval(intervalId);
      console.debug("Clicked play after storage access grant.");
    } catch (e) {}
    numTries++;

    if (numTries >= 50) {
      console.debug("Can not start playback. Giving up.");
      clearInterval(intervalId);
    }
  }, 200);
}

(async () => {
  // Only run the shim for embedded iframes.
  if (window.top == window) {
    return;
  }

  console.warn(
    `When using the Spotify embedded player, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1792395 for details.`
  );

  // Already requested storage access before the reload, trigger playback.
  if (sessionStorage.getItem(AUTOPLAY_FLAG) == "true") {
    sessionStorage.removeItem(AUTOPLAY_FLAG);

    await startFullPlayback();
    return;
  }

  // Wait for the user to click the preview play button. If the player has
  // already loaded the full version, this method will do nothing.
  document.documentElement.addEventListener(
    "click",
    previewPlayButtonListener,
    { capture: true }
  );
})();
