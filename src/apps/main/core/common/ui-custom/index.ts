/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { StyleManager } from "./styles/style-manager.ts";
import { DOMLayoutManager } from "./layout/dom-manipulator.ts";

@noraComponent(import.meta.hot)
export default class UICustomization extends NoraComponentBase {
  private styleManager: StyleManager | null = null;
  private domManager: DOMLayoutManager | null = null;

  init() {
    globalThis.SessionStore.promiseInitialized.then(() => {
      this.styleManager = new StyleManager();
      this.domManager = new DOMLayoutManager();
      this.styleManager?.setupStyleEffects();
      this.domManager?.setupDOMEffects();
    });
  }
}
