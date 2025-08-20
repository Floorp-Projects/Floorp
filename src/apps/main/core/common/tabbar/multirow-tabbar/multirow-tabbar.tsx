/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "../../designs/configs.ts";
import { createEffect, createRoot, getOwner, runWithOwner } from "solid-js";

interface DragData {
  animDropIndex: number;
  movingTabs?: XULElement[];
}

interface TabBrowserTab {
  _dragData: DragData;
}

// Define interfaces for the tab browser elements
interface TabBrowserTabsElement extends XULElement {
  allTabs: XULElement[];
  _positionPinnedTabs(): void;
  _positionPinnedTabs_orig?: () => void;
  on_drop(event: DragEvent): unknown;
  on_drop_orig?: (event: DragEvent) => unknown;
  on_dragover(event: DragEvent): void;
  on_dragover_orig?: (event: DragEvent) => void;
  _handleTabSelect(aInstant: boolean): void;
  _handleTabSelect_orig?: (aInstant: boolean) => void;
  setAttribute(name: string, value: string): void;
  style: CSSStyleDeclaration & {
    paddingInlineStart: string;
    transform: string;
  };
}

interface ArrowScrollboxElement extends XULElement {
  on_wheel?: (event: Event) => void;
  removeEventListener(type: string, listener: EventListener): void;
  addEventListener(type: string, listener: EventListener): void;
}

interface TabBrowserTabsPrototype {
  _positionPinnedTabs(): void;
  _positionPinnedTabs_orig?: () => void;
  on_drop(event: DragEvent): unknown;
  on_drop_orig?: (event: DragEvent) => unknown;
  on_dragover(event: DragEvent): void;
  on_dragover_orig?: (event: DragEvent) => void;
  _handleTabSelect(aInstant: boolean): void;
  _handleTabSelect_orig?: (aInstant: boolean) => void;
  [key: string]: unknown;
}

// Define specific function types for each injection method
type PositionPinnedTabsFunction = (this: TabBrowserTabsElement) => void;
type OnDropFunction = (
  this: TabBrowserTabsElement,
  event: DragEvent,
) => unknown;
type OnDragOverFunction = (
  this: TabBrowserTabsElement,
  event: DragEvent,
) => void;
type HandleTabSelectFunction = (
  this: TabBrowserTabsElement,
  aInstant: boolean,
) => void;

export class MultirowTabbarClass {
  private get arrowScrollbox(): XULElement | null {
    return document?.querySelector("#tabbrowser-arrowscrollbox") || null;
  }

  private get tabsToolbar(): XULElement | null {
    return document?.getElementById("TabsToolbar") as XULElement | null;
  }

  private get tabbrowserTabs(): XULElement | null {
    return document?.getElementById("tabbrowser-tabs") as XULElement | null;
  }

  private get scrollbox(): XULElement | null {
    return this.arrowScrollbox
      ? this.arrowScrollbox.shadowRoot?.querySelector("[part='scrollbox']") ||
        null
      : null;
  }

  private get itemsWrapper(): XULElement | null {
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
    this.removeMultirowTabMaxHeight();
    if (this.isMaxRowEnabled) {
      this.scrollbox?.style.setProperty(
        "max-height",
        `${this.getMultirowTabMaxHeight}px`,
        "important",
      );
      this.arrowScrollbox?.style.removeProperty("max-height");
    }
    // Set max-height to none with !important
    this.tabbrowserTabs?.style.setProperty("max-height", "none", "important");
  }

  private removeMultirowTabMaxHeight() {
    this.scrollbox?.style.removeProperty("max-height");
    this.arrowScrollbox?.style.removeProperty("max-height");
    this.tabbrowserTabs?.style.removeProperty("max-height");
  }

  private enableMultirowTabbar() {
    this.itemsWrapper?.style.setProperty("flex-wrap", "wrap");
    this.itemsWrapper?.style.setProperty("overflow", "scroll");
  }

  private disableMultirowTabbar() {
    this.itemsWrapper?.style.removeProperty("flex-wrap");
    this.itemsWrapper?.style.removeProperty("overflow");
  }

  private applyInjection(): void {
    const win = window;

    const elementConstructor = win.customElements.get("tabbrowser-tabs");
    if (!elementConstructor?.prototype) {
      console.warn("tab box not found in this win");
      return;
    }
    const tabsProto = elementConstructor.prototype as TabBrowserTabsPrototype;
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

    function injectionMethod<T extends keyof TabBrowserTabsPrototype>(
      name: T,
      f: TabBrowserTabsPrototype[T],
    ): void {
      tabsProto[name + "_orig" as string] = tabsProto[name];
      tabsProto[name] = f;
    }

    injectionMethod(
      "_positionPinnedTabs",
      function (this: TabBrowserTabsElement): void {
        // Remove visual offset of pinned tabs
        this._positionPinnedTabs_orig?.();
        this.style.paddingInlineStart = "";
        for (const tab of this.allTabs) {
          (tab.style as CSSStyleDeclaration & { marginInlineStart: string })
            .marginInlineStart = "";
        }
      } as PositionPinnedTabsFunction,
    );

    injectionMethod(
      "on_drop",
      function (this: TabBrowserTabsElement, event: DragEvent): unknown {
        const dt = event.dataTransfer!;
        if (dt.dropEffect !== "move") {
          return this.on_drop_orig?.(event);
        }
        const draggedTab = dt.mozGetDataAt(
          "application/x-moz-tabbrowser-tab",
          0,
        ) as TabBrowserTab;
        draggedTab._dragData.animDropIndex = dropIndex(
          this.allTabs as XULElement[],
          event,
        );
        return this.on_drop_orig?.(event);
      } as OnDropFunction,
    );

    injectionMethod(
      "on_dragover",
      function (this: TabBrowserTabsElement, event: DragEvent): void {
        const dt = event.dataTransfer!;
        if (dt.dropEffect !== "move") {
          return this.on_dragover_orig?.(event);
        }
        const draggedTab = dt.mozGetDataAt(
          "application/x-moz-tabbrowser-tab",
          0,
        ) as TabBrowserTab;
        draggedTab._dragData.animDropIndex = dropIndex(
          this.allTabs as XULElement[],
          event,
        );
        this.on_dragover_orig?.(event);
        // Reset rules that visualize dragging because they don't work in multi-row
        for (const tab of this.allTabs) {
          (tab.style as CSSStyleDeclaration & { transform: string }).transform =
            "";
        }
      } as OnDragOverFunction,
    );

    injectionMethod(
      "_handleTabSelect",
      function (this: TabBrowserTabsElement, aInstant: boolean): void {
        // Only when "overflow" attribute is set, the selected tab will get
        // automatically scrolled into view
        this.setAttribute("overflow", "true");
        this._handleTabSelect_orig?.(aInstant);
      } as HandleTabSelectFunction,
    );

    const tabsElement = win.document!.querySelector(
      "#tabbrowser-tabs",
    ) as TabBrowserTabsElement;
    tabsElement._positionPinnedTabs?.();

    const arrowscrollbox = win.document!.querySelector(
      "#tabbrowser-arrowscrollbox",
    ) as ArrowScrollboxElement;
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
    const tabsProto = elementConstructor.prototype as TabBrowserTabsPrototype;

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
    ) as ArrowScrollboxElement;
    if (arrowscrollbox && arrowscrollbox.on_wheel) {
      arrowscrollbox.addEventListener("wheel", arrowscrollbox.on_wheel);
    }
  }

  constructor() {
    this.tabsToolbar?.setAttribute("multibar", "true");

    const owner = getOwner?.();
    const exec = () =>
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
    if (owner) runWithOwner(owner, exec);
    else createRoot(exec);
  }
}
