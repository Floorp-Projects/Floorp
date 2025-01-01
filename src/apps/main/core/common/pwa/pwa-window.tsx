/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SiteSpecificBrowserManager } from "./ssbManager";
import PwaWindowStyle from "./pwa-window-style.css?inline";
import { render } from "@nora/solid-xul";
import { createSignal, createEffect } from "solid-js";

export class PwaWindowSupport {
  private static instance: PwaWindowSupport;
  private toolbarsDisabled: ReturnType<typeof createSignal<boolean>>;
  private ssbId: ReturnType<typeof createSignal<string | null>>;

  public static getInstance(): PwaWindowSupport {
    if (!PwaWindowSupport.instance) {
      PwaWindowSupport.instance = new PwaWindowSupport();
    }
    return PwaWindowSupport.instance;
  }

  private constructor() {
    this.toolbarsDisabled = createSignal(false);
    this.ssbId = createSignal<string | null>(null);

    const uri = window.arguments?.[0];
    if (!uri?.endsWith("?FloorpEnableSSBWindow=true")) {
      return;
    }

    this.initializeWindow();
    this.setupSignals();
    this.renderStyles();
    this.setupPageActions();
    this.setupTabs();
  }

  private initializeWindow(): void {
    const mainWindow = document?.getElementById("main-window");
    mainWindow?.setAttribute("windowtype", "navigator:ssb-window");
    window.floorpSsbWindow = true;
  }

  private setupSignals(): void {
    const ssbIdAttr = document?.documentElement?.getAttribute("FloorpSSBId") ?? null;
    const [, setSsbId] = this.ssbId;
    setSsbId(ssbIdAttr);

    const [, setToolbarsDisabled] = this.toolbarsDisabled;
    createEffect(() => {
      const disabled = Services.prefs.getBoolPref(
        "floorp.browser.ssb.toolbars.disabled",
        false
      );
      setToolbarsDisabled(disabled);
    });
  }

  private setupPageActions(): void {
    const identityBox = document?.getElementById("identity-box");
    const pageActionBox = document?.getElementById("page-action-buttons");
    if (identityBox && pageActionBox) {
      identityBox.after(pageActionBox);
    }
  }

  private setupTabs(): void {
    window.gBrowser.tabs.forEach((tab: XULElement) => {
      tab.setAttribute("floorpSSB", "true");
    });
  }

  private renderStyles(): void {
    render(() => this.createStyleElement(), document?.head, {
    });
  }

  private createStyleElement() {
    const [toolbarsDisabled] = this.toolbarsDisabled;
    return (
      <style>
        {PwaWindowStyle}
        {toolbarsDisabled()
          ? `
           #nav-bar, #status-bar, #PersonalToolbar, #titlebar {
             display: none;
           }
         `
          : ""}
      </style>
    );
  }

  public get ssbWindowId(): string | null {
    const [ssbId] = this.ssbId;
    return ssbId();
  }

  public async getSsbObj(id: string) {
    return await SiteSpecificBrowserManager.getInstance().getSsbObj(id);
  }
}
