/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global content */

// Common base of DragSourceParentContext and DragTargetParentContext
export class DragParentContextBase {
  // The name of the subtype of this object.
  subtypeName = "";

  // Browsing context that the related element is in
  browsingContext = null;

  constructor(
    aSubtypeName,
    aBrowsingContext,
    aParams,
    aSpecialPowers,
    aOk,
    aIs,
    aInfo
  ) {
    Object.assign(this, aParams);
    this.params = aParams;
    this.subtypeName = aSubtypeName;
    this.browsingContext = aBrowsingContext;
    this.specialPowers = aSpecialPowers;
    this.ok = aOk;
    this.is = aIs;
    this.info = aInfo;
  }

  getElementPositions() {
    return this.runRemote("getElementPositions");
  }

  expect(aMsgType) {
    return this.runRemote("expect", aMsgType);
  }

  checkExpected() {
    return this.runRemote("checkExpected");
  }

  checkHasDrag(aShouldHaveDrag) {
    return this.runRemote("checkHasDrag", aShouldHaveDrag);
  }

  checkSessionHasAction() {
    return this.runRemote("checkSessionHasAction");
  }

  synchronize() {
    return this.runRemoteFn(() => {});
  }

  cleanup() {
    return this.runRemote("cleanup");
  }

  runRemote(aFnName, aParams = []) {
    let args = [this.subtypeName, aFnName].concat(aParams);
    return this.specialPowers.spawn(
      this.browsingContext,
      args,
      (subtypeName, fnName, ...params) => {
        return content.window[subtypeName][fnName](...params);
      }
    );
  }

  runRemoteFn(fn, params = []) {
    return this.specialPowers.spawn(this.browsingContext, params, fn);
  }
}
