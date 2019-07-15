/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

function UpdateSessionStore(aBrowser, aFlushId, aIsFinal, aEpoch, aData) {
  return SessionStoreFuncInternal.updateSessionStore(
    aBrowser,
    aFlushId,
    aIsFinal,
    aEpoch,
    aData
  );
}

var EXPORTED_SYMBOLS = ["UpdateSessionStore"];

var SessionStoreFuncInternal = {
  composeChildren: function SSF_composeScrollPositionsData(
    aPositions,
    aDescendants,
    aStartIndex,
    aNumberOfDescendants
  ) {
    let children = [];
    let lastIndexOfNonNullbject = -1;
    for (let i = 0; i < aNumberOfDescendants; i++) {
      let currentIndex = aStartIndex + i;
      let obj = {};
      let objWithData = false;
      if (aPositions[currentIndex]) {
        obj.scroll = aPositions[currentIndex];
        objWithData = true;
      }
      if (aDescendants[currentIndex]) {
        let descendantsTree = this.composeChildren(
          aPositions,
          aDescendants,
          currentIndex + 1,
          aDescendants[currentIndex]
        );
        i += aDescendants[currentIndex];
        if (descendantsTree) {
          obj.children = descendantsTree;
          objWithData = true;
        }
      }

      if (objWithData) {
        lastIndexOfNonNullbject = children.length;
        children.push(obj);
      } else {
        children.push(null);
      }
    }

    if (lastIndexOfNonNullbject == -1) {
      return null;
    }

    return children.slice(0, lastIndexOfNonNullbject + 1);
  },

  updateScrollPositions: function SSF_updateScrollPositions(
    aPositions,
    aDescendants
  ) {
    let obj = {};
    let objWithData = false;

    if (aPositions[0]) {
      obj.scroll = aPositions[0];
      objWithData = true;
    }

    if (aPositions.length > 1) {
      let children = this.composeChildren(
        aPositions,
        aDescendants,
        1,
        aDescendants[0]
      );
      if (children) {
        obj.children = children;
        objWithData = true;
      }
    }
    if (objWithData) {
      return obj;
    }
    return null;
  },

  updateSessionStore: function SSF_updateSessionStore(
    aBrowser,
    aFlushId,
    aIsFinal,
    aEpoch,
    aData
  ) {
    let currentData = {};
    if (aData.docShellCaps != undefined) {
      currentData.disallow = aData.docShellCaps ? aData.docShellCaps : null;
    }
    if (aData.isPrivate != undefined) {
      currentData.isPrivate = aData.isPrivate;
    }
    if (
      aData.positions != undefined &&
      aData.positionDescendants != undefined
    ) {
      currentData.scroll = this.updateScrollPositions(
        aData.positions,
        aData.positionDescendants
      );
    }
    SessionStore.updateSessionStoreFromTablistener(aBrowser, {
      data: currentData,
      flushID: aFlushId,
      isFinal: aIsFinal,
      epoch: aEpoch,
    });
  },
};
