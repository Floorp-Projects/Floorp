/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  session: "chrome://remote/content/shared/webdriver/Session.sys.mjs",
});

/**
 * Base class for all Root BiDi MessageHandler modules.
 */
export class RootBiDiModule extends Module {
  /**
   * Adds the given node id to the list of seen nodes.
   *
   * @param {object} options
   * @param {BrowsingContext} options.browsingContext
   *     Browsing context the node is part of.
   * @param {string} options.nodeId
   *     Unique id of the node.
   */
  _addNodeToSeenNodes(options = {}) {
    const { browsingContext, nodeId } = options;

    lazy.session.addNodeToSeenNodes(
      this.messageHandler.sessionId,
      browsingContext,
      nodeId
    );
  }
}
