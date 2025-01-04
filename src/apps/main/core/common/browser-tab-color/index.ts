/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


import { onCleanup } from "@nora/solid-xul";
import { TabColorManager } from "./tabcolor-manager";
import chroma from "chroma-js"
import { noraComponent, NoraComponentBase } from "@core/utils/base";
const { ManifestObtainer } = ChromeUtils.importESModule("resource://gre/modules/ManifestObtainer.sys.mjs");

export let manager: TabColorManager;

@noraComponent(import.meta.hot)
export default class BrowserTabColor extends NoraComponentBase {
  init() {
    const _changeTabColor = this.changeTabColor.bind(this);
    const listener = {
      onLocationChange: (_webProgress, _request, _location, _flags) => {
        _changeTabColor();
      },
    } satisfies Pick<nsIWebProgressListener,"onLocationChange">;

    manager = new TabColorManager();
    window.gBrowser.addTabsProgressListener(listener);
    window.gBrowser.tabContainer.addEventListener("TabSelect",this.changeTabColor);
    onCleanup(
      () => {
        window.gBrowser.removeTabsProgressListener(listener);
        window.gBrowser.tabContainer.removeEventListener("TabSelect", this.changeTabColor);
      }
    );
    manager.init();
  }

  changeTabColor() {
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
}

async function getManifest() {
return await ManifestObtainer.browserObtainManifest(window.gBrowser.selectedBrowser);
}
function getTextColor(backgroundColor:string) {
  if (chroma.valid(backgroundColor)) {
    return chroma(backgroundColor).luminance() >= 0.5 ? "black" : "white"
  } else {
    throw Error("[nora@browser-tab-color] theme_color in manifest is not valid");
  }
}