/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DragChildContextBase } from "./DragChildContextBase.sys.mjs";

export class DragTargetChildContext extends DragChildContextBase {
  constructor(aDragWindow, aParams) {
    super("dragTarget", aDragWindow, aParams);
  }

  async checkDropOrDragLeave() {
    let expectedMessage = this.expectDragLeave ? "dragleave" : "drop";
    this.ok(!!this.events[expectedMessage][0], "drop or drag leave existed");
    if (this.events[expectedMessage][0]) {
      this.ok(
        this.nodeIsFlattenedTreeDescendantOf(
          this.events[expectedMessage][0].composedTarget,
          this.dragElement
        ),
        `event target of ${expectedMessage} is targetElement or a descendant`
      );
    }
    await this.checkExpected();
  }
}

export function createDragTargetChildContext(
  aDragWindow,
  aParams,
  aOk,
  aIs,
  aInfo
) {
  aDragWindow.dragTarget = new DragTargetChildContext(aDragWindow, {
    ...aParams,
    ok: aOk,
    is: aIs,
    info: aInfo,
  });
}
