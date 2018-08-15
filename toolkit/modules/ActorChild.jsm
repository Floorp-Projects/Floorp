/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ActorChild"];

/**
 * This should be the base class of any actor class registered via
 * ActorManagerParent and implemented in the child process. It currently takes
 * care of setting the `mm`, `content`, and `docShell` properties based on the
 * message manager it's bound to, but may do more in the future.
 */
class ActorChild {
  constructor(mm) {
    this.mm = mm;
  }

  get content() {
    return this.mm.content;
  }

  get docShell() {
    return this.mm.docShell;
  }
}
