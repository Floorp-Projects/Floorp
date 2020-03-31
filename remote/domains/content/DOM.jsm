/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DOM"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);

class DOM extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
    }
  }

  /**
   * Describes node given its id.
   *
   * Does not require domain to be enabled. Does not start tracking any objects.
   *
   * @param {Object} options
   * @param {number=} options.backendNodeId [not supported]
   *     Identifier of the backend node.
   * @param {number=} options.depth [not supported]
   *     The maximum depth at which children should be retrieved, defaults to 1.
   *     Use -1 for the entire subtree or provide an integer larger than 0.
   * @param {number=} options.nodeId [not supported]
   *     Identifier of the node.
   * @param {string} options.objectId
   *     JavaScript object id of the node wrapper.
   * @param {boolean=} options.pierce [not supported]
   *     Whether or not iframes and shadow roots should be traversed
   *     when returning the subtree, defaults to false.
   *
   * @return {DOM.Node}
   *     Node description.
   */
  describeNode(options = {}) {
    const { objectId } = options;

    // Until nodeId/backendNodeId is supported force usage of the objectId
    if (!["string"].includes(typeof objectId)) {
      throw new TypeError("objectId: string value expected");
    }

    const Runtime = this.session.domains.get("Runtime");
    const debuggerObj = Runtime._getRemoteObject(objectId);
    if (!debuggerObj) {
      throw new Error("Could not find object with given id");
    }

    if (typeof debuggerObj.nodeId == "undefined") {
      throw new Error("Object id doesn't reference a Node");
    }

    const unsafeObj = debuggerObj.unsafeDereference();
    const node = {
      nodeId: debuggerObj.nodeId,
      backendNodeId: debuggerObj.nodeId,
      nodeType: unsafeObj.nodeType,
      nodeName: unsafeObj.nodeName,
      localName: unsafeObj.localName,
      nodeValue: unsafeObj.nodeValue ? unsafeObj.nodeValue.toString() : "",
      childNodeCount: unsafeObj.childElementCount,
      frameId: this.docShell.browsingContext.id.toString(),
    };

    return { node };
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
    }
  }

  getContentQuads({ objectId }) {
    const Runtime = this.session.domains.get("Runtime");
    const debuggerObj = Runtime._getRemoteObject(objectId);
    if (!debuggerObj) {
      throw new Error(`Cannot find object with id: ${objectId}`);
    }
    const unsafeObject = debuggerObj.unsafeDereference();
    if (!unsafeObject.getBoxQuads) {
      throw new Error("RemoteObject is not a node");
    }
    let quads = unsafeObject.getBoxQuads({ relativeTo: this.content.document });
    quads = quads.map(quad => {
      return [
        quad.p1.x,
        quad.p1.y,
        quad.p2.x,
        quad.p2.y,
        quad.p3.x,
        quad.p3.y,
        quad.p4.x,
        quad.p4.y,
      ].map(Math.round);
    });
    return { quads };
  }

  getBoxModel({ objectId }) {
    const Runtime = this.session.domains.get("Runtime");
    const debuggerObj = Runtime._getRemoteObject(objectId);
    if (!debuggerObj) {
      throw new Error(`Cannot find object with id: ${objectId}`);
    }
    const unsafeObject = debuggerObj.unsafeDereference();
    const bounding = unsafeObject.getBoundingClientRect();
    const model = {
      width: Math.round(bounding.width),
      height: Math.round(bounding.height),
    };
    for (const box of ["content", "padding", "border", "margin"]) {
      const quads = unsafeObject.getBoxQuads({
        box,
        relativeTo: this.content.document,
      });

      // getBoxQuads may return more than one element. In this case we have to compute the bounding box
      // of all these boxes.
      let bounding = {
        p1: { x: Infinity, y: Infinity },
        p2: { x: -Infinity, y: Infinity },
        p3: { x: -Infinity, y: -Infinity },
        p4: { x: Infinity, y: -Infinity },
      };
      quads.forEach(quad => {
        bounding = {
          p1: {
            x: Math.min(bounding.p1.x, quad.p1.x),
            y: Math.min(bounding.p1.y, quad.p1.y),
          },
          p2: {
            x: Math.max(bounding.p2.x, quad.p2.x),
            y: Math.min(bounding.p2.y, quad.p2.y),
          },
          p3: {
            x: Math.max(bounding.p3.x, quad.p3.x),
            y: Math.max(bounding.p3.y, quad.p3.y),
          },
          p4: {
            x: Math.min(bounding.p4.x, quad.p4.x),
            y: Math.max(bounding.p4.y, quad.p4.y),
          },
        };
      });

      model[box] = [
        bounding.p1.x,
        bounding.p1.y,
        bounding.p2.x,
        bounding.p2.y,
        bounding.p3.x,
        bounding.p3.y,
        bounding.p4.x,
        bounding.p4.y,
      ].map(Math.round);
    }
    return {
      model,
    };
  }
}
