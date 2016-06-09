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
 * @returns  {nsIDOMWindow}          - the DOMWindow associated to the docShell.
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
 * @returns  {FrameDetail} the FrameDetail JSON object which represents the docShell.
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
 * @param   {nsIDocShell} docShell - the root docShell object
 * @returns {Iterator<DocShell>} the FrameDetail JSON object which represents the docShell.
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
 * Returns the frame ID of the given window. If the window is the
 * top-level content window, its frame ID is 0. Otherwise, its frame ID
 * is its outer window ID.
 *
 * @param {Window} window - The window to retrieve the frame ID for.
 * @returns {number}
 */
function getFrameId(window) {
  let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  if (!docShell.sameTypeParent) {
    return 0;
  }

  let utils = window.getInterface(Ci.nsIDOMWindowUtils);
  return utils.outerWindowID;
}

/**
 * Search for a frame starting from the passed root docShell and
 * convert it to its related frame detail representation.
 *
 * @param  {number}      frameId - the frame ID of the frame to retrieve, as
 *                                 described in getFrameId.
 * @param   {nsIDocShell} rootDocShell - the root docShell object
 * @returns {nsIDocShell?} the docShell with the given frameId, or null
 *                         if no match.
 */
function findDocShell(frameId, rootDocShell) {
  for (let docShell of iterateDocShellTree(rootDocShell)) {
    if (frameId == getFrameId(docShellToWindow(docShell))) {
      return docShell;
    }
  }

  return null;
}

var WebNavigationFrames = {
  iterateDocShellTree,

  findDocShell,

  getFrame(docShell, frameId) {
    let result = findDocShell(frameId, docShell);
    if (result) {
      return convertDocShellToFrameDetail(result);
    }
    return null;
  },

  getFrameId,

  getAllFrames(docShell) {
    return Array.from(iterateDocShellTree(docShell), convertDocShellToFrameDetail);
  },

  getWindowId,
  getParentWindowId,
};
