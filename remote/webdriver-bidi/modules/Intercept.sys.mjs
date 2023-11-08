/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  getSeenNodesForBrowsingContext:
    "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

/**
 * The serialization of JavaScript objects in the content process might produce
 * extra data that needs to be transfered and then processed by the parent
 * process. This extra data is part of the payload as returned by commands
 * and events and can contain the following:
 *
 *     - {Map<BrowsingContext, Array<string>>} seenNodeIds
 *         DOM nodes that need to be added to the navigable seen nodes map.
 *
 * @param {string} sessionId
 *     Id of the WebDriver session
 * @param {object} payload
 *     Payload of the response for the command and event that might contain
 *     a `_extraData` field.
 *
 * @returns {object}
 *     The payload with the extra data removed if it was present.
 */
export function processExtraData(sessionId, payload) {
  // Process extra data if present and delete it from the payload
  if ("_extraData" in payload) {
    const { seenNodeIds } = payload._extraData;

    // Updates the seen nodes for the current session and browsing context.
    seenNodeIds?.forEach((nodeIds, browsingContext) => {
      const seenNodes = lazy.getSeenNodesForBrowsingContext(
        sessionId,
        browsingContext
      );

      nodeIds.forEach(nodeId => seenNodes.add(nodeId));
    });

    delete payload._extraData;
  }

  // Find serialized WindowProxy and resolve browsing context to a navigable id.
  if (payload?.result) {
    payload.result = addContextIdToSerializedWindow(payload.result);
  } else if (payload.exceptionDetails) {
    payload.exceptionDetails = addContextIdToSerializedWindow(
      payload.exceptionDetails
    );
  }

  return payload;
}

function addContextIdToSerializedWindow(serialized) {
  if (serialized.value) {
    switch (serialized.type) {
      case "array":
      case "htmlcollection":
      case "nodelist":
      case "set": {
        serialized.value = serialized.value.map(value =>
          addContextIdToSerializedWindow(value)
        );
        break;
      }

      case "map":
      case "object": {
        serialized.value = serialized.value.map(([key, value]) => [
          key,
          addContextIdToSerializedWindow(value),
        ]);
        break;
      }

      case "window": {
        if (serialized.value.isTopBrowsingContext) {
          const browsingContext = BrowsingContext.getCurrentTopByBrowserId(
            serialized.value.context
          );

          serialized.value = {
            context: lazy.TabManager.getIdForBrowsingContext(browsingContext),
          };
        }
        break;
      }
    }
  } else if (serialized.exception) {
    serialized.exception = addContextIdToSerializedWindow(serialized.exception);
  }

  return serialized;
}
