/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The FrameDetail object which represents a frame in WebExtensions APIs.
 *
 * @typedef  {object}  FrameDetail
 * @inner
 * @property {number}  frameId        - Represents the numeric id which identify the frame in its tab.
 * @property {number}  parentFrameId  - Represents the numeric id which identify the parent frame.
 * @property {string}  url            - Represents the current location URL loaded in the frame.
 * @property {boolean} errorOccurred  - Indicates whether an error is occurred during the last load
 *                                      happened on this frame (NOT YET SUPPORTED).
 */

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
 * Convert a BrowsingContext into internal FrameDetail json.
 *
 * @param {BrowsingContext} bc
 * @returns {FrameDetail}
 */
function getFrameDetail(bc) {
  return {
    frameId: getFrameId(bc),
    parentFrameId: getParentFrameId(bc),
    url: bc.currentURI?.spec,
  };
}

export var WebNavigationFrames = {
  getFrame(bc, frameId) {
    // frameId 0 means the top-level frame; anything else is a child frame.
    let frame = BrowsingContext.get(frameId || bc.id);
    if (frame && frame.top === bc) {
      return getFrameDetail(frame);
    }
    return null;
  },

  getFrameId,
  getParentFrameId,

  getAllFrames(bc) {
    let frames = [];

    // Recursively walk the BC tree, find all frames.
    function visit(bc) {
      frames.push(bc);
      bc.children.forEach(visit);
    }
    visit(bc);
    return frames.map(getFrameDetail);
  },

  getFromWindow(target) {
    if (Window.isInstance(target)) {
      return getFrameId(BrowsingContext.getFromWindow(target));
    }
    return -1;
  },
};
