/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1508639 - Shim Ad Safe Protected's Google IMA adapter
 */

if (!window.googleImaVansAdapter) {
  const shimId = "AdSafeProtectedGoogleIMAAdapter";

  const sendMessageToAddon = (function() {
    const pendingMessages = new Map();
    const channel = new MessageChannel();
    channel.port1.onerror = console.error;
    channel.port1.onmessage = event => {
      const { messageId, response } = event.data;
      const resolve = pendingMessages.get(messageId);
      if (resolve) {
        pendingMessages.delete(messageId);
        resolve(response);
      }
    };
    function reconnect() {
      const detail = {
        pendingMessages: [...pendingMessages.values()],
        port: channel.port2,
        shimId,
      };
      window.dispatchEvent(new CustomEvent("ShimConnects", { detail }));
    }
    window.addEventListener("ShimHelperReady", reconnect);
    reconnect();
    return function(message) {
      const messageId =
        Math.random()
          .toString(36)
          .substring(2) + Date.now().toString(36);
      return new Promise(resolve => {
        const payload = {
          message,
          messageId,
          shimId,
        };
        pendingMessages.set(messageId, resolve);
        channel.port1.postMessage(payload);
      });
    };
  })();

  window.googleImaVansAdapter = {
    init: () => {},
    dispose: () => {},
  };

  if (document.domain === "www.nhl.com") {
    // Treat it as an opt-in when the user clicks on a video
    // TODO: Improve this! It races to tell the bg script to unblock the ad from
    // https://pubads.g.doubleclick.net/gampad/ads before the page loads them.
    async function click(e) {
      if (
        e.isTrusted &&
        e.target.closest(
          "#video-player, .video-preview, article:not([data-video-url=''])"
        )
      ) {
        document.documentElement.removeEventListener("click", click, true);
        await sendMessageToAddon("optIn");
      }
    }
    document.documentElement.addEventListener("click", click, true);
  }
}
