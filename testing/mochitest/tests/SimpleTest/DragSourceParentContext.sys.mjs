/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DragParentContextBase } from "./DragParentContextBase.sys.mjs";

/* global content */
export class DragSourceParentContext extends DragParentContextBase {
  constructor(aBrowsingContext, aParams, aSpecialPowers, aOk, aIs, aInfo) {
    super(
      "dragSource",
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
        let { createDragSourceChildContext } = ChromeUtils.importESModule(
          "chrome://mochikit/content/tests/SimpleTest/DragSourceChildContext.sys.mjs"
        );
        // eslint-disable-next-line no-undef
        createDragSourceChildContext(content.window, params, ok, is, info);
      },
      [this.params]
    );
  }

  checkMouseDown() {
    return this.runRemote("checkMouseDown");
  }

  checkDragStart() {
    return this.runRemote("checkDragStart");
  }

  checkDragEnd() {
    return this.runRemote("checkDragEnd");
  }
}
