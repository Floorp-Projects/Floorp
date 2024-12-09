/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { ContextMenu } from "./context-menu";
import { StatusBar } from "./statusbar";
import { StatusBarManager } from "./statusbar-manager";

export let manager: StatusBarManager;

// function createToolbarWithPlacements(id, placements = [], properties = {}) {
//   //gAddedToolbars.add(id);
//   const tb = document.createXULElement("toolbar");
//   tb.id = id;
//   tb.setAttribute("customizable", "true");

//   properties.type = CustomizableUI.TYPE_TOOLBAR;
//   properties.defaultPlacements = placements;
//   CustomizableUI.registerArea(id, properties);
//   gNavToolbox.appendChild(tb);
//   CustomizableUI.registerToolbarNode(tb);
//   return tb;
// }

export function init() {
  //console.log(createToolbarWithPlacements("nora-aaaa", [], {}));
  createRootHMR(
    () => {
      manager = new StatusBarManager();
    },
    import.meta.hot,
  );
  render(StatusBar, document.body, {
    marker: document?.getElementById("customization-container"),
    hotCtx: import.meta.hot,
  });
  //https://searchfox.org/mozilla-central/rev/4d851945737082fcfb12e9e7b08156f09237aa70/browser/base/content/main-popupset.js#321
  const mainPopupSet = document.getElementById("mainPopupSet");
  mainPopupSet?.addEventListener("popupshowing", onPopupShowing);

  manager.init();

  import.meta.hot?.accept((m) => {
    m?.init();
  });
}

function onPopupShowing(event: Event) {
  switch (event.target.id) {
    case "toolbar-context-menu":
      render(
        ContextMenu,
        document.getElementById("viewToolbarsMenuSeparator")!.parentElement,
        {
          marker: document.getElementById("viewToolbarsMenuSeparator")!,
          hotCtx: import.meta.hot,
        },
      );
  }
}
