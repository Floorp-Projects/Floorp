/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "@core/utils/base";
import { createRoot, getOwner, runWithOwner } from "solid-js";
import { MultirowTabbarClass } from "./multirow-tabbar/multirow-tabbar";
import { TabbarStyleClass } from "./tabbbar-style/tabbar-style";

@noraComponent(import.meta.hot)
export default class TabBar extends NoraComponentBase {
  init() {
    const owner = getOwner();
    globalThis.SessionStore.promiseInitialized.then(() => {
      if (owner) {
        // Ensure the timeout callback runs within Solid owner
        globalThis.setTimeout(() => {
          runWithOwner(owner, () => {
            new TabbarStyleClass();
            new MultirowTabbarClass();
          });
        }, 1000);
      } else {
        createRoot(() => {
          const innerOwner = getOwner();
          globalThis.setTimeout(() => {
            if (innerOwner) {
              runWithOwner(innerOwner, () => {
                new TabbarStyleClass();
                new MultirowTabbarClass();
              });
            } else {
              new TabbarStyleClass();
              new MultirowTabbarClass();
            }
          }, 1000);
        });
      }
    });
  }
}
