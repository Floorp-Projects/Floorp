/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Loads an icon URL into a data URI.
 *
 * @param {Window} window the DOM window providing the icon.
 * @param {string} uri the href for the icon, may be relative to the source page.
 * @returns {Promise<string>} the data URI.
 */
async function loadIcon(window: Window | null, uri: nsIURL) {
  if (!window) {
    return null;
  }

  const iconURL = new window.URL(uri, window.location);
  const request = new window.Request(iconURL, { mode: "cors" });
  request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_IMAGE);

  const response = await window.fetch(request);
  const blob = await response.blob();

  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => resolve(reader.result);
    reader.onerror = reject;
    reader.readAsDataURL(blob);
  });
}

export class NRProgressiveWebAppChild extends JSWindowActorChild {
  async handleEvent(event: Event) {
    // On page show or page hide,
    // check if the page has a manifest and show or hide the page action.
    switch (event.type) {
      case "pageshow":
        this.sendAsyncMessage("ProgressiveWebApp:CheckPageHasManifest");
        return;
    }
  }

  receiveMessage(message: { name: string; data: nsIURL }) {
    switch (message.name) {
      case "LoadIcon":
        return loadIcon(this.contentWindow, message.data);
    }
    return null;
  }
}
