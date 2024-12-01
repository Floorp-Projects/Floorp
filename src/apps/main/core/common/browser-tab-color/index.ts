/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


import { createRootHMR } from "@nora/solid-xul";
import {
  onCleanup
} from "solid-js";
import { TabColorManager } from "./tabcolor-manager";
const { ManifestObtainer } = ChromeUtils.importESModule("resource://gre/modules/ManifestObtainer.sys.mjs");

export let manager: TabColorManager;

export function init() {
  const listener = {
    onLocationChange: (browser, webProgress, request, newURI, flags) => {
      changeTabColor();
    },
  };
  createRootHMR(
    () => {
      manager = new TabColorManager();
      window.gBrowser.addTabsProgressListener(listener);
      window.gBrowser.tabContainer.addEventListener("TabSelect", changeTabColor);
      onCleanup(
        () => {
          window.gBrowser.removeTabsProgressListener(listener);
          window.gBrowser.tabContainer.removeEventListener("TabSelect", changeTabColor);
        });
    },
    import.meta.hot,
  );
  manager.init();
  import.meta.hot?.accept((m) => {
    m?.init();
  });
}

async function getManifest() {
  return await ManifestObtainer.browserObtainManifest(window.gBrowser.selectedBrowser);
}

function getTextColor(backgroundColor: string) {
  let rgb;

  if (backgroundColor.startsWith('#')) {
    const hex = backgroundColor.slice(1);

    if (hex.length === 3 || hex.length === 4) {
      rgb = hex.split('').map(h => Number.parseInt(h + h, 16));
    }
    else if (hex.length === 6 || hex.length === 8) {
      rgb = hex.match(/.{2}/g)?.map(h => Number.parseInt(h, 16));
    }
  }
  else {
    rgb = backgroundColor.match(/\d+/g)?.map(Number);
  }

  if (!rgb || rgb.length < 3) {
    throw new Error("Invalid background color format");
  }

  const brightness = (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) / 1000;

  return brightness >= 128 ? "black" : "white";
}

function changeTabColor() {
  console.log(manager.enableTabColor());
  if (!manager.enableTabColor()) {
    return;
  }
  getManifest().then(res => {
    document?.getElementById("floorp-toolbar-bgcolor")?.remove()
    if (res != null) {
      const elem = document.createElement("style");
      const styleSheet = `
        :root {
            --floorp-tab-panel-bg-color: ${res.theme_color};
            --floorp-tab-panel-fg-color: ${getTextColor(res.theme_color)};
            --floorp-navigator-toolbox-bg-color: var(--floorp-tab-panel-bg-color);
            --floorp-tab-label-fg-color: var(--floorp-tab-panel-fg-color);
            --floorp-tabs-icons-fg-color: var(--floorp-tab-panel-fg-color);
        }

        #browser #TabsToolbar,
        :root:is(:not([lwtheme]), :not(:-moz-lwtheme)) #navigator-toolbox[id],
        #navigator-toolbox[id] {
          background-color: var(--floorp-navigator-toolbox-bg-color) !important;
        }
        .tab-label:not([selected]) {
          color: var(--floorp-tab-label-fg-color) !important;
        }

        .tab-icon-stack > * , #TabsToolbar-customization-target > *, #tabs-newtab-button, .titlebar-color > * {
          color: var(--floorp-tabs-icons-fg-color) !important;
          fill: var(--floorp-tabs-icons-fg-color) !important;
        }
      `;
      elem.textContent = styleSheet;
      elem.id = "floorp-toolbar-bgcolor";
      document?.head?.appendChild(elem);
    }
  })

}