/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { config } from "@core/common/designs/configs.ts";

export class DOMLayoutManager {
  private get navBar(): Element {
    return document?.getElementById("nav-bar") as Element;
  }

  setupDOMEffects() {
    this.setupNavbarPosition();
  }

  private setupNavbarPosition() {
    createEffect(() => {
      if (config().uiCustomization.navbar.position === "bottom") {
        this.moveNavbarToBottom();
      }
    });
  }
  private moveNavbarToBottom() {
    const navbar = this.navBar;

    if (navbar) {
      document?.body?.appendChild(navbar);
    }
  }
}
