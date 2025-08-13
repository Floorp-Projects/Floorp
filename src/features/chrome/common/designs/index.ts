/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { BrowserDesignElement } from "./browser-design-element.tsx";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";

@noraComponent(import.meta.hot)
export default class Designs extends NoraComponentBase {
  init(): void {
    //render(BrowserStyle, document.head);
    render(() => {
      return BrowserDesignElement();
    }, document?.head);

    window.gURLBar.updateLayoutBreakout();
  }
}
