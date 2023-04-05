/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An object that contains details of a stack frame.
 *
 * @typedef {object} StackFrame
 * @see nsIStackFrame
 *
 * @property {string=} asyncCause
 *     Type of asynchronous call by which this frame was invoked.
 * @property {number} columnNumber
 *     The column number for this stack frame.
 * @property {string} filename
 *     The source URL for this stack frame.
 * @property {string} function
 *     SpiderMonkey’s inferred name for this stack frame’s function, or null.
 * @property {number} lineNumber
 *     The line number for this stack frame (starts with 1).
 * @property {number} sourceId
 *     The process-unique internal integer ID of this source.
 */

/**
 * Return a list of stack frames from the given stack.
 *
 * Convert stack objects to the JSON attributes expected by consumers.
 *
 * @param {object} stack
 *     The native stack object to process.
 *
 * @returns {Array<StackFrame>=}
 */
export function getFramesFromStack(stack) {
  if (!stack || (Cu && Cu.isDeadWrapper(stack))) {
    // If the global from which this error came from has been nuked,
    // stack is going to be a dead wrapper.
    return null;
  }

  const frames = [];
  while (stack) {
    frames.push({
      asyncCause: stack.asyncCause,
      columnNumber: stack.column,
      filename: stack.source,
      functionName: stack.functionDisplayName || "",
      lineNumber: stack.line,
      sourceId: stack.sourceId,
    });

    stack = stack.parent || stack.asyncParent;
  }

  return frames;
}

/**
 * Check if a frame is from chrome scope.
 *
 * @param {object} frame
 *     The frame to check
 *
 * @returns {boolean}
 *     True, if frame is from chrome scope
 */
export function isChromeFrame(frame) {
  return (
    frame.filename.startsWith("chrome://") ||
    frame.filename.startsWith("resource://")
  );
}
