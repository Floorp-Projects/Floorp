/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./";
import statusbarStyle from "./statusbar.css?inline";

export function StatusBarElem() {
  return (
    <>
      <xul:toolbar
        id="nora-statusbar"
        toolbarname="Status bar"
        customizable="true"
        style="border-top: 1px solid var(--chrome-content-separator-color)"
        class={`nora-statusbar browser-toolbar ${
          manager.showStatusBar() ? "" : "collapsed"
        } hidden`}
        mode="icons"
        context="toolbar-context-menu"
        accesskey="A"
      >
        <xul:hbox
          id="status-text"
          align="center"
          flex="1"
          class="statusbar-padding"
        />
      </xul:toolbar>
      <style class="nora-statusbar" jsx>
        {statusbarStyle}
      </style>
    </>
  );
}
