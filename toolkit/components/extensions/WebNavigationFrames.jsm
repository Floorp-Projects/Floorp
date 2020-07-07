/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebNavigationFrames"];

/* exported WebNavigationFrames */

/**
 * The FrameDetail object which represents a frame in WebExtensions APIs.
 *
 * @typedef  {Object}  FrameDetail
 * @inner
 * @property {number}  frameId        - Represents the numeric id which identify the frame in its tab.
 * @property {number}  parentFrameId  - Represents the numeric id which identify the parent frame.
 * @property {string}  url            - Represents the current location URL loaded in the frame.
 * @property {boolean} errorOccurred  - Indicates whether an error is occurred during the last load
 *                                      happened on this frame (NOT YET SUPPORTED).
 */

/**
 * A generator function which iterates over a docShell tree, given a root docShell.
 *
 * @param   {nsIDocShell} docShell - the root docShell object
 * @returns {Iterator<nsIDocShell>}
 */
function iterateDocShellTree(docShell) {
  return docShell.getAllDocShellsInSubtree(
    docShell.typeContent,
    docShell.ENUMERATE_FORWARDS
  );
}

/**
 * Returns the frame ID of the given window. If the window is the
 * top-level content window, its frame ID is 0. Otherwise, its frame ID
 * is its outer window ID.
 *
 * @param {Window|BrowsingContext} bc - The window to retrieve the frame ID for.
 * @returns {number}
 */
function getFrameId(bc) {
  if (!BrowsingContext.isInstance(bc)) {
    bc = bc.browsingContext;
  }
  return bc.parent ? bc.id : 0;
}

/**
 * Returns the frame ID of the given window's parent.
 *
 * @param {Window|BrowsingContext} bc - The window to retrieve the parent frame ID for.
 * @returns {number}
 */
function getParentFrameId(bc) {
  if (!BrowsingContext.isInstance(bc)) {
    bc = bc.browsingContext;
  }
  return bc.parent ? getFrameId(bc.parent) : -1;
}

/**
 * Convert a docShell object into its internal FrameDetail representation.
 *
 * @param    {nsIDocShell} docShell - the docShell object to be converted into a FrameDetail JSON object.
 * @returns  {FrameDetail} the FrameDetail JSON object which represents the docShell.
 */
function convertDocShellToFrameDetail(docShell) {
  let { browsingContext, domWindow: window } = docShell;

  return {
    frameId: getFrameId(browsingContext),
    parentFrameId: getParentFrameId(browsingContext),
    url: window.location.href,
  };
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
    if (frameId == getFrameId(docShell.browsingContext)) {
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
  getParentFrameId,

  getAllFrames(docShell) {
    return Array.from(
      iterateDocShellTree(docShell),
      convertDocShellToFrameDetail
    );
  },
};
