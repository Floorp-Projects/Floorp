/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import { PopupElement } from "./popup-element.js";
import workspacesStyles from "./styles.css?inline";
import type { JSX } from "solid-js";
import { WorkspacesService } from "../workspacesService";
import { workspacesDataStore } from "../data/data.js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class WorkspacesToolbarButton {

  private StyleElement = () => {
    return <style>{workspacesStyles}</style>;
  };

  constructor(ctx:WorkspacesService) {
    const gWorkspacesServices = ctx;
    BrowserActionUtils.createMenuToolbarButton(
      "workspaces-toolbar-button",
      "workspaces-toolbar-button",
      "workspacesToolbarButtonPanel",
      <PopupElement ctx={gWorkspacesServices} />,
      null,
      (aNode) => {
        // On Startup, the workspace is not yet loaded, so we need to set the label after the workspace is loaded.
        // We cannot get Element from WorkspacesServices, so we need to get it from CustomizableUI directly.
        const workspace = workspacesDataStore.data.get(workspacesDataStore.selectedID)!;
        aNode?.setAttribute("label", workspace.name);
      },
      CustomizableUI.AREA_TABSTRIP,
      this.StyleElement() as JSX.Element,
      -1,
    );
  }
}
