/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action.tsx";
import { PopupElement } from "./popup-element.tsx";
import workspacesStyles from "./styles.css?inline";
import { createEffect, type JSX, onCleanup } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { configStore, enabled } from "../data/config.ts";
import Workspaces from "../index.ts";
import { selectedWorkspaceID } from "../data/data.ts";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

let lastDisplayedWorkspaceId: string | null = null;

export class WorkspacesToolbarButton {
  private StyleElement = () => {
    return <style>{workspacesStyles}</style>;
  };

  constructor(ctx: WorkspacesService) {
    BrowserActionUtils.createMenuToolbarButton(
      "workspaces-toolbar-button",
      "workspaces-toolbar-button",
      "workspacesToolbarButtonPanel",
      <PopupElement ctx={ctx} />,
      null,
      () => {
        setTimeout(() => this.updateButtonIfNeeded(true), 500);

        createEffect(() => {
          const _ = configStore.showWorkspaceNameOnToolbar;
          const __ = enabled();
          this.updateButtonIfNeeded();
        });

        createEffect(() => {
          const _ = selectedWorkspaceID();
          this.updateButtonIfNeeded();
        });
      },
      CustomizableUI.AREA_TABSTRIP,
      this.StyleElement() as JSX.Element,
      0,
    );

    const intervalId = setInterval(() => this.updateButtonIfNeeded(), 500);
    onCleanup(() => clearInterval(intervalId));
  }

  private getButtonNode(): Element | null {
    if (!document) return null;
    return document.querySelector("#workspaces-toolbar-button");
  }

  private updateButtonIfNeeded(force = false) {
    const aNode = this.getButtonNode();
    if (!aNode) return;

    try {
      const ctx = Workspaces.getCtx();
      if (!ctx) return;

      const currentId = ctx.getSelectedWorkspaceID();
      if (!currentId) return;

      if (!force && lastDisplayedWorkspaceId === currentId) return;

      lastDisplayedWorkspaceId = currentId;

      const workspace = ctx.getRawWorkspace(currentId);
      const icon = ctx.iconCtx.getWorkspaceIconUrl(workspace.icon);
      const xulElement = aNode as unknown as XULElement;

      xulElement.style.setProperty(
        "list-style-image",
        icon ? `url(${icon})` : `url("chrome://branding/content/icon32.png")`,
      );

      if (configStore.showWorkspaceNameOnToolbar) {
        xulElement.setAttribute("label", workspace.name);
      } else {
        xulElement.removeAttribute("label");
      }
      if (!enabled()) {
        xulElement.setAttribute("hidden", "true");
      } else {
        xulElement.removeAttribute("hidden");
      }
    } catch (e) {
      console.error("Error updating workspace toolbar button:", e);
    }
  }
}
