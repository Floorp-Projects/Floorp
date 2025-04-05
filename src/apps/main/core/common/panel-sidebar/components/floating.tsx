/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import {
  isFloating,
  panelSidebarConfig,
  setPanelSidebarConfig,
  setSelectedPanelId,
  setIsFloatingDragging,
  selectedPanelId,
} from "../data/data.ts";
import { STATIC_PANEL_DATA } from "../data/static-panels.ts";
import { isResizeCooldown } from "./floating-splitter.tsx";
import type { Panel } from "../utils/type";

declare global {
  interface Window {
    gFloorpPanelSidebar?: {
      getPanelData: (id: string) => Panel | undefined;
      showPanel: (panel: Panel) => void;
    };
  }
}

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

export class PanelSidebarFloating {
  private static instance: PanelSidebarFloating;
  public static getInstance() {
    if (!PanelSidebarFloating.instance) {
      PanelSidebarFloating.instance = new PanelSidebarFloating();
    }
    return PanelSidebarFloating.instance;
  }

  private resizeObserver: ResizeObserver | null = null;
  private parentHeightTargetId = "tabbrowser-tabbox";
  private userResizedHeight = false;
  private isDraggingHeader = false;

  constructor() {
    createEffect(() => {
      if (isFloating()) {
        if (!this.userResizedHeight) {
          this.applyHeightToSidebarBox();
        }
        this.initResizeObserver();
        this.initDragHeader();
        this.applyStoredPositionToSidebarBox();
        // document?.addEventListener("click", this.handleOutsideClick);
      } else {
        this.removeFloatingStyles();
        this.resizeObserver?.disconnect();
        // document?.removeEventListener("click", this.handleOutsideClick);
        this.userResizedHeight = false;
        this.restoreActivePanel();
      }
    });

    createEffect(() => {
      const position = panelSidebarConfig().position_start;
      if (position) {
        document
          ?.getElementById("panel-sidebar-box")
          ?.setAttribute("data-floating-splitter-side", "start");
      } else {
        document
          ?.getElementById("panel-sidebar-box")
          ?.setAttribute("data-floating-splitter-side", "end");
      }
    });
  }

  private initResizeObserver() {
    const tabbrowserTabboxElem = document?.getElementById(
      this.parentHeightTargetId,
    );
    const sidebarBox = document?.getElementById("panel-sidebar-box");

    if (!tabbrowserTabboxElem || !sidebarBox) {
      return;
    }

    this.resizeObserver = new ResizeObserver((entries) => {
      for (const entry of entries) {
        if (entry.target.id === this.parentHeightTargetId && isFloating() && !this.userResizedHeight) {
          this.applyHeightToSidebarBox();
        }
      }
    });

    this.resizeObserver.observe(tabbrowserTabboxElem);

    sidebarBox.addEventListener("mousedown", (e: MouseEvent) => {
      const target = e.target as HTMLElement;
      const isResizer =
        target.classList.contains("floating-splitter-side") ||
        target.classList.contains("floating-splitter-vertical") ||
        target.classList.contains("floating-splitter-corner");

      if (isResizer) {
        this.userResizedHeight = true;

        const onMouseUp = () => {
          this.saveCurrentSidebarSize();
          document?.removeEventListener("mouseup", onMouseUp);
        };

        document?.addEventListener("mouseup", onMouseUp);
      }
    });
  }

  private initDragHeader() {
    const header = document?.getElementById("panel-sidebar-header") as XULElement;
    const sidebarBox = document?.getElementById("panel-sidebar-box") as XULElement;

    if (!header || !sidebarBox) {
      return;
    }

    header.style.setProperty("cursor", "move");

    header.addEventListener("mousedown", (e: MouseEvent) => {
      if (this.isDraggingHeader) {
        return;
      }

      if ((e.target as HTMLElement).tagName === "button" ||
        (e.target as HTMLElement).tagName === "BUTTON" ||
        (e.target as HTMLElement).closest(".panel-sidebar-actions")) {
        return;
      }

      e.preventDefault();
      this.isDraggingHeader = true;
      setIsFloatingDragging(true);

      const startX = e.clientX;
      const startY = e.clientY;
      const startLeft = parseInt(sidebarBox.style.getPropertyValue("left") || "0", 10) ||
        sidebarBox.getBoundingClientRect().left;
      const startTop = parseInt(sidebarBox.style.getPropertyValue("top") || "0", 10) ||
        sidebarBox.getBoundingClientRect().top;

      sidebarBox.style.setProperty("margin", "0");
      sidebarBox.style.setProperty("position", "fixed");
      sidebarBox.style.setProperty("left", `${startLeft}px`);
      sidebarBox.style.setProperty("top", `${startTop}px`);

      const onMouseMove = (e: MouseEvent) => {
        const deltaX = e.clientX - startX;
        const deltaY = e.clientY - startY;

        const newLeft = Math.max(0, Math.min(window.innerWidth - sidebarBox.clientWidth, startLeft + deltaX));
        const newTop = Math.max(0, Math.min(window.innerHeight - sidebarBox.clientHeight, startTop + deltaY));

        sidebarBox.style.setProperty("left", `${newLeft}px`);
        sidebarBox.style.setProperty("top", `${newTop}px`);
      };

      const onMouseUp = () => {
        this.isDraggingHeader = false;
        setIsFloatingDragging(false);
        document?.removeEventListener("mousemove", onMouseMove);
        document?.removeEventListener("mouseup", onMouseUp);

        this.savePosition();
      };

      document?.addEventListener("mousemove", onMouseMove);
      document?.addEventListener("mouseup", onMouseUp);
    });
  }

  private savePosition() {
    const sidebarBox = document?.getElementById("panel-sidebar-box") as XULElement;
    if (!sidebarBox) {
      return;
    }

    const left = parseInt(sidebarBox.style.getPropertyValue("left") || "0", 10);
    const top = parseInt(sidebarBox.style.getPropertyValue("top") || "0", 10);

    const config = panelSidebarConfig();
    setPanelSidebarConfig({
      ...config,
      floatingPositionLeft: left,
      floatingPositionTop: top
    });
  }

  private applyHeightToSidebarBox() {
    (document?.getElementById("panel-sidebar-box") as XULElement).style.height =
      `${this.getBrowserHeight() - 20}px`;
  }

  private removeFloatingStyles() {
    const sidebarBox = document?.getElementById("panel-sidebar-box") as XULElement;
    if (!sidebarBox) {
      return;
    }

    sidebarBox.style.removeProperty("height");
    sidebarBox.style.removeProperty("width");
    sidebarBox.style.removeProperty("position");
    sidebarBox.style.removeProperty("left");
    sidebarBox.style.removeProperty("top");
    sidebarBox.style.removeProperty("margin");

    sidebarBox.style.setProperty("min-width", "225px");
  }

  private removeHeightToSidebarBox() {
    (document?.getElementById("panel-sidebar-box") as XULElement).style.height =
      "";
  }

  private getBrowserHeight() {
    return (
      document?.getElementById(this.parentHeightTargetId)?.clientHeight ?? 0
    );
  }

  private saveCurrentSidebarSize() {
    const sidebarBox = document?.getElementById("panel-sidebar-box") as XULElement;
    if (!sidebarBox) return;

    const config = panelSidebarConfig();

    const width = sidebarBox.getBoundingClientRect().width;
    const height = sidebarBox.getBoundingClientRect().height;

    setPanelSidebarConfig({
      ...config,
      floatingWidth: width,
      floatingHeight: height
    });
  }

  private applyStoredPositionToSidebarBox() {
    const config = panelSidebarConfig();
    const sidebarBox = document?.getElementById("panel-sidebar-box") as XULElement;

    if (!sidebarBox) {
      return;
    }

    if (config.floatingPositionLeft !== undefined && config.floatingPositionTop !== undefined) {
      sidebarBox.style.setProperty("margin", "0");
      sidebarBox.style.setProperty("position", "fixed");

      const width = config.floatingWidth || sidebarBox.getBoundingClientRect().width;
      const height = config.floatingHeight || sidebarBox.getBoundingClientRect().height;
      const left = Math.max(0, Math.min(window.innerWidth - width, config.floatingPositionLeft));
      const top = Math.max(0, Math.min(window.innerHeight - height, config.floatingPositionTop));

      sidebarBox.style.setProperty("left", `${left}px`);
      sidebarBox.style.setProperty("top", `${top}px`);
    } else {
      const defaultLeft = 20;
      const defaultTop = 100;
      sidebarBox.style.setProperty("margin", "0");
      sidebarBox.style.setProperty("position", "fixed");
      sidebarBox.style.setProperty("left", `${defaultLeft}px`);
      sidebarBox.style.setProperty("top", `${defaultTop}px`);
    }

    if (config.floatingWidth !== undefined && config.floatingHeight !== undefined) {
      sidebarBox.style.setProperty("width", `${config.floatingWidth}px`);
      sidebarBox.style.setProperty("height", `${config.floatingHeight}px`);
      this.userResizedHeight = true;
    }
  }

  private handleOutsideClick = (event: MouseEvent) => {
    if (!isFloating()) {
      return;
    }

    if (isResizeCooldown() || this.isDraggingHeader) {
      return;
    }

    const sidebarBox = document?.getElementById("panel-sidebar-box");
    const selectBox = document?.getElementById("panel-sidebar-select-box");
    const splitter = document?.getElementById("panel-sidebar-splitter");
    const browsers = sidebarBox?.querySelectorAll(".sidebar-panel-browser");

    const clickedBrowser = (event.target as XULElement).ownerDocument
      ?.activeElement;
    const clickedBrowserIsSidebarBrowser = Array.from(browsers ?? []).some(
      (browser) => browser === clickedBrowser,
    );
    const clickedElementIsChromeSidebar = Object.values(STATIC_PANEL_DATA).some(
      (panel) =>
        panel.url === (clickedBrowser as XULElement).ownerDocument?.documentURI,
    );
    const clickedElementIsWebTypeBrowser = clickedBrowser?.baseURI?.startsWith(
      `${AppConstants.BROWSER_CHROME_URL}?floorpWebPanelId`,
    );
    const insideSidebar = sidebarBox?.contains(event.target as Node) ||
      clickedBrowserIsSidebarBrowser;
    const insideSelectBox = selectBox?.contains(event.target as Node);
    const insideSplitter = splitter?.contains(event.target as Node);

    if (
      !insideSidebar &&
      !insideSelectBox &&
      !insideSplitter &&
      !clickedElementIsChromeSidebar &&
      !clickedElementIsWebTypeBrowser
    ) {
      setSelectedPanelId(null);
    }
  };

  private restoreActivePanel() {
    const currentPanelId = selectedPanelId();

    if (currentPanelId) {
      try {
        const panelSidebarInstance = window.gFloorpPanelSidebar;
        if (panelSidebarInstance) {
          setSelectedPanelId(null);

          setTimeout(() => {
            setSelectedPanelId(currentPanelId);
            if (panelSidebarInstance.showPanel) {
              const panel = panelSidebarInstance.getPanelData(currentPanelId);
              if (panel) {
                panelSidebarInstance.showPanel(panel);
              }
            }
          }, 50);
        }
      } catch (e) {
        console.error("Failed to restore panel:", e);
      }
    }
  }
}
