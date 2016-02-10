/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebNavigationFrames"];

var Ci = Components.interfaces;

/* exported WebNavigationFrames */

function getWindowId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .outerWindowID;
}

function getParentWindowId(window) {
  return getWindowId(window.parent);
}

/**
 * Retrieve the DOMWindow associated to the docShell passed as parameter.
 *
 * @param    {nsIDocShell}  docShell - the docShell that we want to get the DOMWindow from.
 * @return   {nsIDOMWindow}          - the DOMWindow associated to the docShell.
 */
function docShellToWindow(docShell) {
  return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindow);
}

/**
 * The FrameDetail object which represents a frame in WebExtensions APIs.
 *
 * @typedef  {Object}  FrameDetail
 * @inner
 * @property {number}  windowId       - Represents the numeric id which identify the frame in its tab.
 * @property {number}  parentWindowId - Represents the numeric id which identify the parent frame.
 * @property {string}  url            - Represents the current location URL loaded in the frame.
 * @property {boolean} errorOccurred  - Indicates whether an error is occurred during the last load
 *                                      happened on this frame (NOT YET SUPPORTED).
 */

/**
 * Convert a docShell object into its internal FrameDetail representation.
 *
 * @param    {nsIDocShell} docShell - the docShell object to be converted into a FrameDetail JSON object.
 * @return   {FrameDetail} the FrameDetail JSON object which represents the docShell.
 */
function convertDocShellToFrameDetail(docShell) {
  let window = docShellToWindow(docShell);

  return {
    windowId: getWindowId(window),
    parentWindowId: getParentWindowId(window),
    url: window.location.href,
  };
}

/**
 * A generator function which iterates over a docShell tree, given a root docShell.
 *
 * @param  {nsIDocShell} docShell - the root docShell object
 * @return {Iterator<DocShell>} the FrameDetail JSON object which represents the docShell.
 */
function* iterateDocShellTree(docShell) {
  let docShellsEnum = docShell.getDocShellEnumerator(
    Ci.nsIDocShellTreeItem.typeContent,
    Ci.nsIDocShell.ENUMERATE_FORWARDS
  );

  while (docShellsEnum.hasMoreElements()) {
    yield docShellsEnum.getNext();
  }

  return null;
}

/**
 * Search for a frame starting from the passed root docShell and
 * convert it to its related frame detail representation.
 *
 * @param  {number}      windowId - the windowId of the frame to retrieve
 * @param  {nsIDocShell} docShell - the root docShell object
 * @return {FrameDetail} the FrameDetail JSON object which represents the docShell.
 */
function findFrame(windowId, rootDocShell) {
  for (let docShell of iterateDocShellTree(rootDocShell)) {
    if (windowId == getWindowId(docShellToWindow(docShell))) {
      return convertDocShellToFrameDetail(docShell);
    }
  }

  return null;
}

var WebNavigationFrames = {
  getFrame(docShell, frameId) {
    if (frameId == 0) {
      return convertDocShellToFrameDetail(docShell);
    }

    return findFrame(frameId, docShell);
  },

  getAllFrames(docShell) {
    return Array.from(iterateDocShellTree(docShell), convertDocShellToFrameDetail);
  },

  getWindowId,
  getParentWindowId,
};
