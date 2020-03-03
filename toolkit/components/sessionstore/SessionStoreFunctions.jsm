/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

function UpdateSessionStore(
  aBrowser,
  aFlushId,
  aIsFinal,
  aEpoch,
  aData,
  aCollectSHistory
) {
  return SessionStoreFuncInternal.updateSessionStore(
    aBrowser,
    aFlushId,
    aIsFinal,
    aEpoch,
    aData,
    aCollectSHistory
  );
}

var EXPORTED_SYMBOLS = ["UpdateSessionStore"];

var SessionStoreFuncInternal = {
  // form data which is waiting to be updated
  _formDataId: [],
  _formDataIdValue: [],
  _formDataXPath: [],
  _formDataXPathValue: [],

  /**
   * The data will be stored in the arrays:
   *   "_formDataId, _formDataIdValue" for the elements with id.
   *   "_formDataXPath, _formDataXPathValue" for the elements with XPath.
   */
  updateFormData: function SSF_updateFormData(aType, aData) {
    let idArray = this._formDataId;
    let valueArray = this._formDataIdValue;

    if (aType == "XPath") {
      idArray = this._formDataXPath;
      valueArray = this._formDataXPathValue;
    }

    let valueIdx = aData.valueIdx;
    for (let i = 0; i < aData.id.length; i++) {
      idArray.push(aData.id[i]);

      if (aData.type[i] == "singleSelect") {
        valueArray.push({
          selectedIndex: aData.selectedIndex[valueIdx[i]],
          value: aData.selectVal[valueIdx[i]],
        });
      } else if (aData.type[i] == "file") {
        valueArray.push({
          type: "file",
          fileList: aData.strVal.slice(valueIdx[i], valueIdx[++i]),
        });
      } else if (aData.type[i] == "multipleSelect") {
        valueArray.push(aData.strVal.slice(valueIdx[i], valueIdx[++i]));
      } else if (aData.type[i] == "string") {
        valueArray.push(aData.strVal[valueIdx[i]]);
      } else if (aData.type[i] == "bool") {
        valueArray.push(aData.boolVal[valueIdx[i]]);
      }
    }
  },

  /**
   * Return the array of formdata for this._sessionData.formdata.children
   *
   * aStartIndex:       Current index for aInnerHTML/aUrl/aNumId/aNumXPath/aDescendants.
   *                    (aStartIndex means the index of current root frame)
   * aInnerHTML:        Array for innerHTML.
   * aUrl:              Array for url.
   * aNumId:            Array for number of containing elements with id
   * aNumXPath:         Array for number of containing elements with XPath
   * aDescendants:      Array for number of descendants.
   *
   * aCurrentIdIdx:     Current index for this._formDataId and this._formDataIdValue
   * aCurrentXPathIdx:  Current index for this._formDataXPath and this._formDataXPathValue
   * aNumberOfDescendants: The number of descendants for current frame
   *
   * The returned array includes "aNumberOfDescendants" formdata objects.
   */
  composeInputChildren: function SSF_composeInputChildren(
    aInnerHTML,
    aUrl,
    aCurrentIdIdx,
    aNumId,
    aCurrentXpathIdx,
    aNumXPath,
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

      // set url/id/xpath
      if (aUrl[currentIndex]) {
        obj.url = aUrl[currentIndex];
        objWithData = true;

        if (aInnerHTML[currentIndex]) {
          // eslint-disable-next-line no-unsanitized/property
          obj.innerHTML = aInnerHTML[currentIndex];
        }
        if (aNumId[currentIndex]) {
          let idObj = {};
          for (let idx = 0; idx < aNumId[currentIndex]; idx++) {
            idObj[
              this._formDataId[aCurrentIdIdx + idx]
            ] = this._formDataIdValue[aCurrentIdIdx + idx];
          }
          obj.id = idObj;
        }

        // We want to avoid saving data for about:sessionrestore as a string.
        // Since it's stored in the form as stringified JSON, stringifying further
        // causes an explosion of escape characters. cf. bug 467409
        if (
          obj.url == "about:sessionrestore" ||
          obj.url == "about:welcomeback"
        ) {
          obj.id.sessionData = JSON.parse(obj.id.sessionData);
        }

        if (aNumXPath[currentIndex]) {
          let xpathObj = {};
          for (let idx = 0; idx < aNumXPath[currentIndex]; idx++) {
            xpathObj[
              this._formDataXPath[aCurrentXpathIdx + idx]
            ] = this._formDataXPathValue[aCurrentXpathIdx + idx];
          }
          obj.xpath = xpathObj;
        }
      }

      // compose the descendantsTree which will be pushed into children array
      if (aDescendants[currentIndex]) {
        let descendantsTree = this.composeInputChildren(
          aInnerHTML,
          aUrl,
          aCurrentIdIdx + aNumId[currentIndex],
          aNumId,
          aCurrentXpathIdx + aNumXPath[currentIndex],
          aNumXPath,
          aDescendants,
          currentIndex + 1,
          aDescendants[currentIndex]
        );
        i += aDescendants[currentIndex];
        if (descendantsTree) {
          obj.children = descendantsTree;
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

  /**
   * Update the object for this._sessionData.formdata.
   * The object contains the formdata for all reachable frames.
   *
   * "object.children" is an array with one entry per frame,
   * containing formdata as a nested data structure according
   * to the layout of the frame tree, or null if no formdata.
   *
   * Example:
   *   {
   *     url: "http://mozilla.org/",
   *     id: {input_id: "input value"},
   *     xpath: {input_xpath: "input value"},
   *     children: [
   *       null,
   *       {url: "http://sub.mozilla.org/", id: {input_id: "input value 2"}}
   *     ]
   *   }
   *
   * Each index of the following array is corresponging to each frame.
   * aDescendants: Array for number of descendants
   * aInnerHTML:   Array for innerHTML
   * aUrl:         Array for url
   * aNumId:       Array for number of containing elements with id
   * aNumXPath:    Array for number of containing elements with XPath
   *
   * Here we use [index 0] to compose the formdata object of root frame.
   * Besides, we use composeInputChildren() to get array of "object.children".
   */
  updateInput: function SSF_updateInput(
    aSessionData,
    aDescendants,
    aInnerHTML,
    aUrl,
    aNumId,
    aNumXPath
  ) {
    let obj = {};
    let objWithData = false;

    if (aUrl[0]) {
      obj.url = aUrl[0];

      if (aInnerHTML[0]) {
        // eslint-disable-next-line no-unsanitized/property
        obj.innerHTML = aInnerHTML[0];
        objWithData = true;
      }

      if (aNumId[0]) {
        let idObj = {};
        for (let i = 0; i < aNumId[0]; i++) {
          idObj[this._formDataId[i]] = this._formDataIdValue[i];
        }
        obj.id = idObj;
        objWithData = true;
      }

      // We want to avoid saving data for about:sessionrestore as a string.
      // Since it's stored in the form as stringified JSON, stringifying further
      // causes an explosion of escape characters. cf. bug 467409
      if (obj.url == "about:sessionrestore" || obj.url == "about:welcomeback") {
        obj.id.sessionData = JSON.parse(obj.id.sessionData);
      }

      if (aNumXPath[0]) {
        let xpathObj = {};
        for (let i = 0; i < aNumXPath[0]; i++) {
          xpathObj[this._formDataXPath[i]] = this._formDataXPathValue[i];
        }
        obj.xpath = xpathObj;
        objWithData = true;
      }
    }

    if (aDescendants.length > 1) {
      let descendantsTree = this.composeInputChildren(
        aInnerHTML,
        aUrl,
        aNumId[0],
        aNumId,
        aNumXPath[0],
        aNumXPath,
        aDescendants,
        1,
        aDescendants[0]
      );
      if (descendantsTree) {
        obj.children = descendantsTree;
        objWithData = true;
      }
    }

    if (objWithData) {
      aSessionData.formdata = obj;
    } else {
      aSessionData.formdata = null;
    }
  },

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

  updateStorage: function SSF_updateStorage(aOrigins, aKeys, aValues) {
    let data = {};
    for (let i = 0; i < aOrigins.length; i++) {
      // If the key isn't defined, then .clear() was called, and we send
      // up null for this domain to indicate that storage has been cleared
      // for it.
      if (aKeys[i] == "") {
        while (aOrigins[i + 1] == aOrigins[i]) {
          i++;
        }
        data[aOrigins[i]] = null;
      } else {
        let hostData = {};
        hostData[aKeys[i]] = aValues[i];
        while (aOrigins[i + 1] == aOrigins[i]) {
          i++;
          hostData[aKeys[i]] = aValues[i];
        }
        data[aOrigins[i]] = hostData;
      }
    }
    if (aOrigins.length) {
      return data;
    }

    return null;
  },

  updateSessionStore: function SSF_updateSessionStore(
    aBrowser,
    aFlushId,
    aIsFinal,
    aEpoch,
    aData,
    aCollectSHistory
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
    if (aData.id != undefined) {
      this.updateFormData("id", aData.id);
    }
    if (aData.xpath != undefined) {
      this.updateFormData("XPath", aData.xpath);
    }
    if (aData.inputDescendants != undefined) {
      this.updateInput(
        currentData,
        aData.inputDescendants,
        aData.innerHTML,
        aData.url,
        aData.numId,
        aData.numXPath
      );
    }
    if (aData.isFullStorage != undefined) {
      let storage = this.updateStorage(
        aData.storageOrigins,
        aData.storageKeys,
        aData.storageValues
      );
      if (aData.isFullStorage) {
        currentData.storage = storage;
      } else {
        currentData.storagechange = storage;
      }
    }

    SessionStore.updateSessionStoreFromTablistener(aBrowser, {
      data: currentData,
      flushID: aFlushId,
      isFinal: aIsFinal,
      epoch: aEpoch,
      sHistoryNeeded: aCollectSHistory,
    });
    this._formDataId = [];
    this._formDataIdValue = [];
    this._formDataXPath = [];
    this._formDataXPathValue = [];
  },
};
