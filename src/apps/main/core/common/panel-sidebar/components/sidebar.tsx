/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import style from "./style.css?inline";
import { SidebarHeader } from "./sidebar-header";
import { SidebarSelectbox } from "./sidebar-selectbox";
import { SidebarSplitter } from "./sidebar-splitter";
import { createEffect, Show } from "solid-js";
import {
  selectedPanelId,
  isFloating,
  isFloatingDragging,
  isPanelSidebarEnabled,
} from "../data/data";
import { FloatingSplitter } from "./floating-splitter";
import { BrowserBox } from "./browser-box";
import { CPanelSidebar } from "./panel-sidebar";

export class PanelSidebarElem {

  ctx: CPanelSidebar

  private get documentElement() {
    return document?.documentElement as XULElement;
  }

  constructor(ctx:CPanelSidebar) {
    this.ctx = ctx;
    if (!isPanelSidebarEnabled()) {
      return;
    }
    const parentElem = document?.getElementById("browser");
    const beforeElem = document?.getElementById("tabbrowser-tabbox");
    render(() => this.sidebar(), parentElem, {
      marker: beforeElem as XULElement,
    });

    render(() => this.style(), document?.head);

    createEffect(() => {
      if (selectedPanelId() === null) {
        this.documentElement?.style.setProperty(
          "--panel-sidebar-display",
          "none",
        );
      } else {
        this.documentElement?.style.setProperty(
          "--panel-sidebar-display",
          "flex",
        );
      }
    });
  }

  private style() {
    return <style>{style}</style>;
  }

  private sidebar() {
    return (
      <>
        <xul:vbox
          id="panel-sidebar-box"
          class="chromeclass-extrachrome"
          data-floating={isFloating().toString()}
        >
          <SidebarHeader ctx={this.ctx} />
          <BrowserBox />
          <Show when={isFloating()}>
            <FloatingSplitter />
          </Show>
        </xul:vbox>
        <Show when={!isFloating()}>
          <SidebarSplitter />
        </Show>
        <SidebarSelectbox ctx={this.ctx} />
      </>
    );
  }
}
