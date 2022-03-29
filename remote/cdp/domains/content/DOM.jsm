/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DOM"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ContentProcessDomain:
    "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm",
});

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

    const attributes = [];
    if (unsafeObj.attributes) {
      // Flatten the list of attributes for name and value
      for (const attribute of unsafeObj.attributes) {
        attributes.push(attribute.name, attribute.value);
      }
    }

    let context = this.docShell.browsingContext;
    if (unsafeObj instanceof HTMLIFrameElement) {
      context = unsafeObj.contentWindow.docShell.browsingContext;
    }

    const node = {
      nodeId: debuggerObj.nodeId,
      backendNodeId: debuggerObj.nodeId,
      nodeType: unsafeObj.nodeType,
      nodeName: unsafeObj.nodeName,
      localName: unsafeObj.localName,
      nodeValue: unsafeObj.nodeValue ? unsafeObj.nodeValue.toString() : "",
      childNodeCount: unsafeObj.childElementCount,
      attributes: attributes.length > 0 ? attributes : undefined,
      frameId: context.id.toString(),
    };

    return { node };
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
    }
  }

  getContentQuads(options = {}) {
    const { objectId } = options;
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

  getBoxModel(options = {}) {
    const { objectId } = options;
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

  /**
   * Resolves the JavaScript node object for a given NodeId or BackendNodeId.
   *
   * @param {Object} options
   * @param {number} options.backendNodeId [required for now]
   *     Backend identifier of the node to resolve.
   * @param {number=} options.executionContextId
   *     Execution context in which to resolve the node.
   * @param {number=} options.nodeId [not supported]
   *     Id of the node to resolve.
   * @param {string=} options.objectGroup [not supported]
   *     Symbolic group name that can be used to release multiple objects.
   *
   * @return {Runtime.RemoteObject}
   *     JavaScript object wrapper for given node.
   */
  resolveNode(options = {}) {
    const { backendNodeId, executionContextId } = options;

    // Until nodeId is supported force usage of the backendNodeId
    // Bug 1625417 - CDP expects the id as number
    if (!["string"].includes(typeof backendNodeId)) {
      throw new TypeError("backendNodeId: string value expected");
    }
    if (!["undefined", "number"].includes(typeof executionContextId)) {
      throw new TypeError("executionContextId: integer value expected");
    }

    const Runtime = this.session.domains.get("Runtime");

    // Retrieve the node to resolve, and its context
    const debuggerObj = Runtime._getRemoteObjectByNodeId(backendNodeId);

    if (!debuggerObj) {
      throw new Error(`No node with given id found`);
    }

    // If execution context isn't specified use the default one for the node
    let context;
    if (typeof executionContextId != "undefined") {
      context = Runtime.contexts.get(executionContextId);
      if (!context) {
        throw new Error(`Node with given id does not belong to the document`);
      }
    } else {
      context = Runtime._getDefaultContextForWindow();
    }

    Runtime._setRemoteObject(debuggerObj, context);

    return {
      object: Runtime._serializeRemoteObject(debuggerObj, context.id),
    };
  }
}
