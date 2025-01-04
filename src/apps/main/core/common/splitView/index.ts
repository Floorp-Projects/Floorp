/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { CSplitView } from "./splitView";
import { SplitViewManager } from "./manager";
import { SplitViewContextMenu } from "./tabContextMenu";
import { noraComponent, NoraComponentBase } from "@core/utils/base";

//TODO: refactor needed
@noraComponent(import.meta.hot)
export default class SplitView extends NoraComponentBase {
  init() {
    const ctx = new CSplitView();
    new SplitViewManager(ctx);
    new SplitViewContextMenu(ctx);
  }
}
