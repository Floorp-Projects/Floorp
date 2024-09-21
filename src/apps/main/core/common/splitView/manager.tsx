/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "solid-js/web";
import { Popup } from "./popup";

export class SplitViewManager {
  private static instance: SplitViewManager;
  public static getInstance() {
    if (!SplitViewManager.instance) {
      SplitViewManager.instance = new SplitViewManager();
    }
    return SplitViewManager.instance;
  }

  private constructor() {
    render(() => this.ToolbarElement(), this.targetParent);
  }

  private get targetParent() {
    return document?.getElementById("identity-box") as XULElement;
  }

  private ToolbarElement() {
    return (
      <xul:hbox
        data-l10n-id="splitView-action"
        class="urlbar-page-action"
        role="button"
        popup="splitView-panel"
        id="splitView-action"
        hidden={true}
      >
        <xul:image id="splitView-image" class="urlbar-icon" />
        <Popup />
      </xul:hbox>
    );
  }
}
