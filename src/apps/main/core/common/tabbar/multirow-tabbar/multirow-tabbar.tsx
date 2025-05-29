/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs.ts";
import { createEffect } from "solid-js";

interface DragData {
  animDropIndex: number;
  movingTabs?: XULElement[];
}

interface TabBrowserTab {
  _dragData: DragData;
}

export class MultirowTabbarClass {
  private get arrowScrollbox(): XULElement | null {
    return document?.querySelector("#tabbrowser-arrowscrollbox") || null;
  }

  private get tabsToolbar(): XULElement | null {
    return document?.getElementById("TabsToolbar") as XULElement | null;
  }

  private get scrollboxPart(): XULElement | null {
    return this.arrowScrollbox
      ? this.arrowScrollbox.shadowRoot?.querySelector(
        "[part='items-wrapper']",
      ) || null
      : null;
  }

  private get aTabHeight(): number {
    return (
      document?.querySelector(".tabbrowser-tab:not([hidden='true'])")
        ?.clientHeight || 30
    );
  }

  private get isMaxRowEnabled(): boolean {
    return config().tabbar.multiRowTabBar.maxRowEnabled;
  }
  private get getMultirowTabMaxHeight(): number {
    return config().tabbar.multiRowTabBar.maxRow * this.aTabHeight;
  }

  private setMultirowTabMaxHeight() {
    if (!this.isMaxRowEnabled) {
      return;
    }
    this.scrollboxPart?.setAttribute(
      "style",
      `max-height: ${this.getMultirowTabMaxHeight}px !important;`,
    );
    this.arrowScrollbox?.style.setProperty(
      "max-height",
      `${this.getMultirowTabMaxHeight}px`,
    );
  }

  private removeMultirowTabMaxHeight() {
    this.scrollboxPart?.removeAttribute("style");
    this.arrowScrollbox?.style.removeProperty("max-height");
  }

  private enableMultirowTabbar() {
    this.scrollboxPart?.setAttribute("style", "flex-wrap: wrap;");
  }

  private disableMultirowTabbar() {
    this.scrollboxPart?.removeAttribute("style");
  }

  private applyInjection(): void {
    const win = window;

    const elementConstructor = win.customElements.get("tabbrowser-tabs");
    if (!elementConstructor?.prototype) {
      console.warn("tab box not found in this win");
      return;
    }
    const tabsProto = elementConstructor.prototype as any;
    if (tabsProto._positionPinnedTabs_orig) {
      console.warn("tab box already injectioned in this win");
      return;
    }

    const dropIndex = (tabs: XULElement[], event: DragEvent): number => {
      for (const tab of tabs) {
        const rect = tab.getBoundingClientRect();
        if (
          event.screenY >= tab.screenY &&
          event.screenY < tab.screenY + rect.height &&
          event.screenX < tab.screenX + rect.width / 2
        ) {
          // First tab right of the cursor, and in the cursor's row
          return tabs.indexOf(tab);
        }
        if (event.screenY <= tab.screenY) {
          // Entered a new tab row
          return tabs.indexOf(tab);
        }
      }
      return -1;
    };

    function injectionMethod(
      name: string,
      f: (this: any, ...args: any[]) => any,
    ): void {
      tabsProto[name + "_orig"] = tabsProto[name];
      tabsProto[name] = f;
    }

    injectionMethod("_positionPinnedTabs", function (this: any): void {
      // Remove visual offset of pinned tabs
      this._positionPinnedTabs_orig();
      this.style.paddingInlineStart = "";
      for (const tab of this.allTabs) {
        tab.style.marginInlineStart = "";
      }
    });

    injectionMethod("on_drop", function (this: any, event: DragEvent): any {
      const dt = event.dataTransfer!;
      if (dt.dropEffect !== "move") {
        return this.on_drop_orig(event);
      }
      const draggedTab = dt.mozGetDataAt(
        "application/x-moz-tabbrowser-tab",
        0,
      ) as TabBrowserTab;
      draggedTab._dragData.animDropIndex = dropIndex(
        this.allTabs as XULElement[],
        event,
      );
      return this.on_drop_orig(event);
    });

    injectionMethod(
      "on_dragover",
      function (this: any, event: DragEvent): void {
        const dt = event.dataTransfer!;
        if (dt.dropEffect !== "move") {
          return this.on_dragover_orig(event);
        }
        const draggedTab = dt.mozGetDataAt(
          "application/x-moz-tabbrowser-tab",
          0,
        ) as TabBrowserTab;
        draggedTab._dragData.animDropIndex = dropIndex(
          this.allTabs as XULElement[],
          event,
        );
        this.on_dragover_orig(event);
        // Reset rules that visualize dragging because they don't work in multi-row
        for (const tab of this.allTabs) {
          tab.style.transform = "";
        }
      },
    );

    injectionMethod(
      "_handleTabSelect",
      function (this: any, aInstant: boolean): void {
        // Only when "overflow" attribute is set, the selected tab will get
        // automatically scrolled into view
        this.setAttribute("overflow", "true");
        this._handleTabSelect_orig(aInstant);
      },
    );

    const tabsElement = win.document!.querySelector("#tabbrowser-tabs") as any;
    tabsElement._positionPinnedTabs?.();

    const arrowscrollbox = win.document!.querySelector(
      "#tabbrowser-arrowscrollbox",
    ) as any;
    if (arrowscrollbox && arrowscrollbox.on_wheel) {
      arrowscrollbox.removeEventListener("wheel", arrowscrollbox.on_wheel);
    }
  }

  private removeInjection(): void {
    const win = window;

    const elementConstructor = win.customElements.get("tabbrowser-tabs");
    if (!elementConstructor?.prototype) {
      console.warn("tab box not injectioned");
      return;
    }
    const tabsProto = elementConstructor.prototype as any;

    function uninjectionMethod(name: string): void {
      if (tabsProto[name + "_orig"]) {
        tabsProto[name] = tabsProto[name + "_orig"];
        delete tabsProto[name + "_orig"];
      }
    }

    uninjectionMethod("_positionPinnedTabs");
    uninjectionMethod("on_drop");
    uninjectionMethod("on_dragover");
    uninjectionMethod("_handleTabSelect");

    const arrowscrollbox = win.document!.querySelector(
      "#tabbrowser-arrowscrollbox",
    ) as any;
    if (arrowscrollbox && arrowscrollbox.on_wheel) {
      arrowscrollbox.addEventListener("wheel", arrowscrollbox.on_wheel);
    }
  }

  constructor() {
    this.tabsToolbar?.setAttribute("multibar", "true");

    createEffect(() => {
      if (config().tabbar.tabbarStyle === "multirow") {
        this.enableMultirowTabbar();
        this.setMultirowTabMaxHeight();
        this.applyInjection();
      } else {
        this.disableMultirowTabbar();
        this.removeMultirowTabMaxHeight();
        this.removeInjection();
      }
    });
  }
}
