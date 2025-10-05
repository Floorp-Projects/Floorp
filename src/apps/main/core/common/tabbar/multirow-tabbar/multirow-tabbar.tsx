/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, onCleanup } from "solid-js";
import { MULTIROW_TABBAR_BASE_CSS } from "./multirow-tabbar-css.ts";
import { getTabsToolbar, resolveTabsContainer } from "./dom-utils.ts";
import { PinnedTabController } from "./pinned-tab-controller.ts";
import { TabDragDropManager } from "./tab-drag-drop-manager.ts";
import { config } from "../../designs/configs.ts";

// deno-lint-ignore no-explicit-any
declare function makeURI(uri: string): any;
declare const window: Window & {
  getComputedStyle(element: Element): CSSStyleDeclaration;
};

export class MultirowTabbarClass {
  private readonly pinnedTabs = new PinnedTabController(resolveTabsContainer);
  private readonly dragDropManager = new TabDragDropManager(
    resolveTabsContainer,
    this.pinnedTabs,
  );
  private styleElement: HTMLStyleElement | null = null;
  // deno-lint-ignore no-explicit-any
  private agentSheetUri: any = null;

  constructor() {
    getTabsToolbar()?.setAttribute("multibar", "true");
    this.initMultiRowTabs();
    this.setupReactiveMaxRows();
  }

  private initMultiRowTabs(): void {
    const arrowScrollbox = resolveTabsContainer();
    if (!arrowScrollbox) {
      return;
    }

    this.injectCSS(arrowScrollbox);
    this.dragDropManager.install(arrowScrollbox);
  }

  private setupReactiveMaxRows(): void {
    const arrowScrollbox = resolveTabsContainer();
    if (!arrowScrollbox) {
      return;
    }

    createEffect(() => {
      const multiRowConfig = config().tabbar.multiRowTabBar;
      const maxRowEnabled = multiRowConfig.maxRowEnabled ?? false;
      const maxRows = multiRowConfig.maxRow ?? 3;

      this.updateMaxRowsCSS(arrowScrollbox, maxRowEnabled, maxRows);
    });

    onCleanup(() => {
      this.cleanup();
    });
  }

  private updateMaxRowsCSS(
    _arrowScrollbox: XULElement,
    maxRowEnabled: boolean,
    maxRows: number,
  ): void {
    if (this.agentSheetUri) {
      try {
        const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
          Ci.nsIStyleSheetService,
        );
        if (sss.sheetRegistered(this.agentSheetUri, sss.AGENT_SHEET)) {
          sss.unregisterSheet(this.agentSheetUri, sss.AGENT_SHEET);
        }
      } catch (e) {
        console.error("Failed to unregister sheet:", e);
      }
    }

    const css = this.generateMaxRowsCSS(maxRowEnabled, maxRows);
    const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
      Ci.nsIStyleSheetService,
    );
    this.agentSheetUri = makeURI(
      "data:text/css;charset=UTF=8," + encodeURIComponent(css),
    );
    sss.loadAndRegisterSheet(this.agentSheetUri, sss.AGENT_SHEET);
  }

  private generateMaxRowsCSS(maxRowEnabled: boolean, maxRows: number): string {
    let css = MULTIROW_TABBAR_BASE_CSS;

    css += `
      scrollbar, #tab-scrollbox-resizer {-moz-window-dragging: no-drag !important}

      #tabbrowser-tabs > arrowscrollbox {
        overflow: visible;
        display: block;`;

    if (maxRowEnabled && maxRows > 0) {
      const tabElement = document?.querySelector(".tabbrowser-tab");
      const tabHeight = tabElement?.clientHeight ?? 33;
      const maxHeight = tabHeight * maxRows;
      css += `
        max-height: ${maxHeight}px;
        overflow-y: auto;`;
    }

    css += `
      }
    `;

    return css;
  }

  private cleanup(): void {
    if (this.agentSheetUri) {
      try {
        const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
          Ci.nsIStyleSheetService,
        );
        if (sss.sheetRegistered(this.agentSheetUri, sss.AGENT_SHEET)) {
          sss.unregisterSheet(this.agentSheetUri, sss.AGENT_SHEET);
        }
      } catch (e) {
        console.error("Failed to cleanup sheet:", e);
      }
    }
  }

  private injectCSS(arrowScrollbox: XULElement): void {
    if (!document || !window) return;

    this.pinnedTabs.register();
    const style = document.createElement("style");
    const scrollboxStyle = `
        .scrollbox-clip {
            overflow: visible;
            display: block}

        scrollbox {
            display: flex;
            flex-wrap: wrap;
            min-height: var(--tab-min-height);
        }

        scrollbox > slot {
            flex-wrap: wrap;
        }

	    .arrowscrollbox-overflow-start-indicator,
	    .arrowscrollbox-overflow-end-indicator {position: fixed !important}

	    .scrollbutton-up, .scrollbutton-down, spacer,
        #scrollbutton-up, #scrollbutton-down {display: none !important}
	    `;

    style.innerHTML = scrollboxStyle;
    arrowScrollbox?.shadowRoot?.appendChild(style);
    this.styleElement = style;
  }
}
