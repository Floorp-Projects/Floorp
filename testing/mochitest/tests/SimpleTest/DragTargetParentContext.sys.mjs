/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Target Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DragParentContextBase } from "./DragParentContextBase.sys.mjs";

/* global content */
export class DragTargetParentContext extends DragParentContextBase {
  constructor(aBrowsingContext, aParams, aSpecialPowers, aOk, aIs, aInfo) {
    super(
      "dragTarget",
      aBrowsingContext,
      aParams,
      aSpecialPowers,
      aOk,
      aIs,
      aInfo
    );
  }

  initialize() {
    return this.runRemoteFn(
      params => {
        let { createDragTargetChildContext } = ChromeUtils.importESModule(
          "chrome://mochikit/content/tests/SimpleTest/DragTargetChildContext.sys.mjs"
        );
        // eslint-disable-next-line no-undef
        createDragTargetChildContext(content.window, params, ok, is, info);
      },
      [this.params]
    );
  }

  checkDropOrDragLeave() {
    return this.runRemote("checkDropOrDragLeave");
  }
}
