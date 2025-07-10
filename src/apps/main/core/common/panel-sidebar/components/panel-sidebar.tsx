/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { ChromeSiteBrowser } from "../browsers/chrome-site-browser";
import { ExtensionSiteBrowser } from "../browsers/extension-site-browser";
import { WebSiteBrowser } from "../browsers/web-site-browser";
import {
  panelSidebarConfig,
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarData,
  setSelectedPanelId,
} from "../data/data";
import type { Panel } from "../utils/type";
import { createEffect } from "solid-js";
import { getExtensionSidebarAction } from "../extension-panels";
import { WebsitePanel } from "../website-panel-window-parent";
import "../utils/webRequest.ts";

export class CPanelSidebar {
  private get parentElement() {
    return document?.getElementById("panel-sidebar-browser-box") as
      | XULElement
      | undefined;
  }

  private get sidebarElement() {
    return document?.getElementById("panel-sidebar-box") as
      | XULElement
      | undefined;
  }

  private get browsers() {
    return document?.querySelectorAll(".sidebar-panel-browser") as
      | NodeListOf<XULElement>
      | undefined;
  }

  constructor() {
    createEffect(() => {
      const currentCheckedPanels = Array.from(
        document?.querySelectorAll(".panel-sidebar-panel[data-checked]") ?? [],
      );
      currentCheckedPanels.map((panel) => {
        panel.removeAttribute("data-checked");
      });

      const currentPanel = this.getPanelData(selectedPanelId() ?? "");
      if (currentPanel) {
        document
          ?.querySelector(`.panel-sidebar-panel[id="${currentPanel.id}"]`)
          ?.setAttribute("data-checked", "true");
      }
    });
  }

  public getBrowserElement(id: string) {
    return document?.getElementById(`sidebar-panel-${id}`) as
      | (XULElement & {
        contentWindow: Window;
        goBack: () => void;
        goForward: () => void;
        goIndex: () => void;
        reload: () => void;
        toggleMute: () => void;
      })
      | undefined;
  }

  public getPanelData(id: string): Panel | undefined {
    return panelSidebarData().find((panel) => panel.id === id);
  }

  private createBrowserComponent(panel: Panel) {
    const components = {
      web: WebSiteBrowser,
      extension: ExtensionSiteBrowser,
      static: ChromeSiteBrowser,
    };

    const BrowserComponent = components[panel.type];
    if (!BrowserComponent) {
      throw new Error(`Unsupported panel type: ${panel.type}`);
    }

    return <BrowserComponent {...panel} />;
  }

  private resetBrowsersFlex(): void {
    Array.from(this.browsers ?? []).forEach((browser) => {
      browser.removeAttribute("flex");
    });
  }

  private renderBrowserComponent(panel: Panel): void {
    if (!this.parentElement) {
      throw new Error("Parent element not found");
    }

    const browserComponent = this.createBrowserComponent(panel);
    render(() => browserComponent, this.parentElement, {
      // biome-ignore lint/suspicious/noExplicitAny: Required for hot module replacement
      hotCtx: (import.meta as any).hot,
    });

    this.initBrowser(panel);
  }

  private initBrowser(panel: Panel) {
    if (panel.type === "extension") {
      const browser = this.getBrowserElement(panel.id) as XULElement & {
        contentWindow: Window;
      };

      if (!browser) {
        throw new Error("Browser element not found");
      }

      if (!panel.extensionId) {
        throw new Error("Extension ID not found");
      }

      const sidebarAction = getExtensionSidebarAction(panel.extensionId);

      browser.addEventListener("DOMContentLoaded", () => {
        const oa = window.E10SUtils.predictOriginAttributes({ browser });
        browser.setAttribute(
          "remoteType",
          window.E10SUtils.getRemoteTypeForURI(
            panel.url ?? "",
            true,
            false,
            window.E10SUtils.EXTENSION_REMOTE_TYPE,
            null,
            oa,
          ),
        );

        browser.contentWindow.loadPanel(
          panel.extensionId,
          sidebarAction.default_panel,
          "sidebar",
        );
      });
    }
  }

  public changePanel(panelId: string): void {
    if (panelId === selectedPanelId()) {
      setSelectedPanelId(null);
      if (panelSidebarConfig().autoUnload) {
        this.unloadPanel(panelId);
      }
      return;
    }

    const panel = this.getPanelData(panelId);
    if (!panel) {
      throw new Error(`Panel not found: ${panelId}`);
    }

    setSelectedPanelId(panelId);
    this.setSidebarWidth(panel);
    this.resetBrowsersFlex();
    this.showPanel(panel);
  }

  public showPanel(panel: Panel): void {
    const browser = this.getBrowserElement(panel.id);
    if (browser) {
      browser.setAttribute("flex", "1");
      return;
    }
    this.renderBrowserComponent(panel);
  }

  public saveCurrentSidebarWidth() {
    const currentWidth = this.sidebarElement?.getAttribute("width");
    if (currentWidth) {
      setPanelSidebarData((prev) =>
        prev.map((panel) =>
          panel.id === selectedPanelId()
            ? { ...panel, width: Number(currentWidth) }
            : panel
        )
      );
    }
  }

  private setSidebarWidth(panel: Panel) {
    this.sidebarElement?.style.setProperty(
      "width",
      `${panel.width !== 0 ? panel.width : panelSidebarConfig().globalWidth}px`,
    );
  }

  public openInMainWindow(panelId: string) {
    const url = this.getPanelData(panelId)?.url;
    window.gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      inBackground: false,
    });
  }

  public deletePanel(panelId: string) {
    this.unloadPanel(panelId);
    setPanelSidebarData((prev) => prev.filter((p) => p.id !== panelId));
  }

  public unloadPanel(panelId: string) {
    const browser = this.getBrowserElement(panelId);
    if (browser) {
      browser.remove();
    }

    setSelectedPanelId(null);
  }

  public mutePanel(panelId: string) {
    const gWebsitePanel = WebsitePanel.getInstance();
    gWebsitePanel.toggleMutePanel(panelId);
  }

  public changeZoomLevel(panelId: string, type: "in" | "out" | "reset") {
    const gWebsitePanel = WebsitePanel.getInstance();

    switch (type) {
      case "in":
        gWebsitePanel.zoomInPanel(panelId);
        break;
      case "out":
        gWebsitePanel.zoomOutPanel(panelId);
        break;
      case "reset":
        gWebsitePanel.resetZoomLevelPanel(panelId);
        break;
    }
  }
}
