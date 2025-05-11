/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./";
import statusbarUtilsStyle from "./statusbar-utils.css?inline";

export function StatusBarElem() {
  return (
    <>
      <xul:toolbar
        id="nora-statusbar"
        toolbarname="Status bar"
        customizable="true"
        class={`border-t border-[var(--chrome-content-separator-color)] ${
          manager.showStatusBar() ? "flex items-center" : "hidden"
        } browser-toolbar w-full !bg-[var(--panel-sidebar-background-color)]`}
        mode="icons"
        context="toolbar-context-menu"
        accesskey="A"
      >
        <xul:hbox
          id="status-text"
          align="center"
          flex="1"
          class="px-2 overflow-hidden"
        />
      </xul:toolbar>
      <style class="nora-statusbar-utils" jsx>
        {statusbarUtilsStyle}
      </style>
    </>
  );
}
