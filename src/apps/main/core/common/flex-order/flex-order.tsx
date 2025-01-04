/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { createSignal, onCleanup } from "solid-js";
import { panelSidebarConfig } from "../panel-sidebar/data/data";

type Orders = {
  fxSidebar: number,
  fxSidebarSplitter: number,
  browserBox: number,
  floorpSidebarSplitter: number,
  floorpSidebar: number,
  floorpSidebarSelectBox: number,
}

export namespace gFlexOrder {
  const fxSidebarPosition = "sidebar.position_start";
  const fxSidebarId = "sidebar-box";
  const fxSidebarSplitterId = "sidebar-splitter";

  const floorpSidebarId = "panel-sidebar-box";
  const floorpSidebarSplitterId = "panel-sidebar-splitter";
  const floorpSidebarSelectBoxId = "panel-sidebar-select-box";
  const browserBoxId = "tabbrowser-tabbox";

  const [orders,setOrders] = createRootHMR(()=>createSignal<Orders>({fxSidebar:-1,fxSidebarSplitter:-1,browserBox:-1,floorpSidebarSplitter:-1,floorpSidebar:-1,floorpSidebarSelectBox:-1}),import.meta.hot)

  export function init() {
    applyFlexOrder();
    renderOrderStyle()
    Services.prefs.addObserver(fxSidebarPosition, applyFlexOrder);

    onCleanup(()=>{
      Services.prefs.removeObserver(fxSidebarPosition, applyFlexOrder);
    });
  }

  export function applyFlexOrder() {
    const fxSidebarPositionPref = Services.prefs.getBoolPref(fxSidebarPosition);
    const floorpSidebarPositionPref = panelSidebarConfig().position_start;

    if (fxSidebarPositionPref && floorpSidebarPositionPref) {
      // Fx's sidebar -> browser -> Floorp's sidebar
      setOrders({
        fxSidebar: 0,
        fxSidebarSplitter: 1,
        browserBox: 2,
        floorpSidebarSplitter: 3,
        floorpSidebar: 4,
        floorpSidebarSelectBox: 5,
      });
    } else if (fxSidebarPositionPref && !floorpSidebarPositionPref) {
      // Floorp sidebar -> Fx's sidebar -> browser
      setOrders({
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        browserBox: 5,
      });
    } else if (!fxSidebarPositionPref && floorpSidebarPositionPref) {
      // browser -> Vertical tab bar -> Fx's sidebar -> Floorp's sidebar
      setOrders({
        browserBox: 0,
        verticaltabbarSplitter: 1,
        verticaltabbar: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        floorpSidebarSplitter: 5,
        floorpSidebar: 6,
        floorpSidebarSelectBox: 7,
      });
    } else {
      // Floorp's sidebar -> browser -> Vertical tab bar -> Fx's sidebar
      setOrders({
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        browserBox: 3,
        verticaltabbarSplitter: 4,
        verticaltabbar: 5,
        fxSidebar: 6,
        fxSidebarSplitter: 7,
      });
    }
  }

  function renderOrderStyle() {
    render(() => <style jsx>{`
      #${fxSidebarId} {
        order: ${orders().fxSidebar} !important;
      }
      #${floorpSidebarId} {
        order: ${orders().floorpSidebar} !important;
      }
      #${floorpSidebarSelectBoxId} {
        order: ${orders().floorpSidebarSelectBox} !important;
      }
      #${floorpSidebarSplitterId} {
        order: ${orders().floorpSidebarSplitter} !important;
      }
      #${fxSidebarSplitterId} {
        order: ${orders().fxSidebarSplitter} !important;
      }
      #${browserBoxId} {
        order: ${orders().browserBox} !important;
      }
    `}</style>, document?.head);
  }
}
