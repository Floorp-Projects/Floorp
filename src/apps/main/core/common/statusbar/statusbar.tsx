/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./";
import statusbarStyle from "./statusbar.css?inline";

export function StatusBarElem() {
  return (
    <>
      <div class="card card-border bg-base-100 w-96">
        <div class="card-body">
          <h2 class="card-title">Card Title</h2>
          <p>A card component has a figure, a body part, and inside body there are title and actions parts</p>
          <div class="card-actions justify-end">
            <button class="btn btn-primary">Buy Now</button>
          </div>
        </div>
      </div>
      <xul:toolbar
        id="nora-statusbar"
        toolbarname="Status bar"
        customizable="true"
        style="border-top: 1px solid var(--chrome-content-separator-color)"
        class={`nora-statusbar browser-toolbar ${manager.showStatusBar() ? "" : "collapsed"
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
