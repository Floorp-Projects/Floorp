/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { createEffect } from "solid-js";
import { panelSidebarConfig } from "../panel-sidebar/data";

export namespace gFlexOrder {
  const fxSidebarPosition = "sidebar.position_start";
  const fxSidebarId = "sidebar-box";
  const fxSidebarSplitterId = "sidebar-splitter";

  const floorpSidebarId = "panel-sidebar-box";
  const floorpSidebarSplitterId = "panel-sidebar-splitter";
  const floorpSidebarSelectBoxId = "panel-sidebar-select-box";
  const browserBoxId = "appcontent";

  export function init() {
    applyFlexOrder();
    Services.prefs.addObserver(fxSidebarPosition, applyFlexOrder);

    createEffect(() => {
      panelSidebarConfig();
      applyFlexOrder();
    });
  }

  export function applyFlexOrder() {
    const fxSidebarPositionPref = Services.prefs.getBoolPref(fxSidebarPosition);
    const floorpSidebarPositionPref = panelSidebarConfig().position_start;

    if (fxSidebarPositionPref && floorpSidebarPositionPref) {
      // Fx's sidebar -> browser -> Floorp's sidebar
      const orders = {
        fxSidebar: 0,
        fxSidebarSplitter: 1,
        browserBox: 2,
        floorpSidebarSplitter: 3,
        floorpSidebar: 4,
        floorpSidebarSelectBox: 5,
      };
      renderOrderStyle(orders);
    } else if (fxSidebarPositionPref && !floorpSidebarPositionPref) {
      // Floorp sidebar -> Fx's sidebar -> browser
      const orders = {
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        browserBox: 5,
      };
      renderOrderStyle(orders);
    } else if (!fxSidebarPositionPref && floorpSidebarPositionPref) {
      // browser -> Vertical tab bar -> Fx's sidebar -> Floorp's sidebar
      const orders = {
        browserBox: 0,
        verticaltabbarSplitter: 1,
        verticaltabbar: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        floorpSidebarSplitter: 5,
        floorpSidebar: 6,
        floorpSidebarSelectBox: 7,
      };
      renderOrderStyle(orders);
    } else {
      // Floorp's sidebar -> browser -> Vertical tab bar -> Fx's sidebar
      const orders = {
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        browserBox: 3,
        verticaltabbarSplitter: 4,
        verticaltabbar: 5,
        fxSidebar: 6,
        fxSidebarSplitter: 7,
      };
      renderOrderStyle(orders);
    }
  }

  function createOrderStyle(
    fxSidebar: number,
    fxSidebarSplitter: number,
    floorpSidebar: number,
    floorpSidebarSplitter: number,
    floorpSidebarSelectBox: number,
    browserBox: number,
  ) {
    return (
      <style jsx>{`
        #${fxSidebarId} {
          order: ${fxSidebar} !important;
        }
        #${floorpSidebarId} {
          order: ${floorpSidebar} !important;
        }
        #${floorpSidebarSelectBoxId} {
          order: ${floorpSidebarSelectBox} !important;
        }
        #${floorpSidebarSplitterId} {
          order: ${floorpSidebarSplitter} !important;
        }
        #${fxSidebarSplitterId} {
          order: ${fxSidebarSplitter} !important;
        }
        #${browserBoxId} {
          order: ${browserBox} !important;
        }
      `}</style>
    );
  }

  function renderOrderStyle(orders: {
    fxSidebar: number;
    fxSidebarSplitter: number;
    floorpSidebar: number;
    floorpSidebarSplitter: number;
    floorpSidebarSelectBox: number;
    browserBox: number;
  }) {
    const style = createOrderStyle(
      orders.fxSidebar,
      orders.fxSidebarSplitter,
      orders.floorpSidebar,
      orders.floorpSidebarSplitter,
      orders.floorpSidebarSelectBox,
      orders.browserBox,
    );

    render(() => style, document?.head, {
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      hotCtx: (import.meta as any)?.hot,
    });
  }
}
