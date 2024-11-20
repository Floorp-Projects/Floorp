/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "@core/common/designs/configs";
import { createEffect } from "solid-js";
import style from "./style.css?inline";
import { render } from "@nora/solid-xul";

export class TabPinnedTabCustomization {
  private static instance: TabPinnedTabCustomization;
  public static getInstance() {
    if (!TabPinnedTabCustomization.instance) {
      TabPinnedTabCustomization.instance = new TabPinnedTabCustomization();
    }
    return TabPinnedTabCustomization.instance;
  }

  private dispose: (() => void) | null = null;

  private StyleElement() {
    return <style>{style}</style>;
  }

  private toggleTitleVisibility(showTitleEnabled: boolean) {
    if (showTitleEnabled) {
      this.dispose = render(() => this.StyleElement(), document?.head);
    } else {
      this.dispose?.();
    }
  }

  constructor() {
    createEffect(() => {
      const showTitleEnabled = config().tab.tabPinTitle;
      this.toggleTitleVisibility(showTitleEnabled);
    });
  }
}
