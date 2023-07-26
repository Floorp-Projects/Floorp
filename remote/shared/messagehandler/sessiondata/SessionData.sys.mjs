/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

/**
 * @typedef {string} SessionDataCategory
 */

/**
 * Enum of session data categories.
 *
 * @readonly
 * @enum {SessionDataCategory}
 */
export const SessionDataCategory = {
  Event: "event",
  PreloadScript: "preload-script",
};

/**
 * @typedef {string} SessionDataMethod
 */

/**
 * Enum of session data methods.
 *
 * @readonly
 * @enum {SessionDataMethod}
 */
export const SessionDataMethod = {
  Add: "add",
  Remove: "remove",
};

export const SESSION_DATA_SHARED_DATA_KEY = "MessageHandlerSessionData";

// This is a map from session id to session data, which will be persisted and
// propagated to all processes using Services' sharedData.
// We have to store this as a unique object under a unique shared data key
// because new MessageHandlers in other processes will need to access this data
// without any notion of a specific session.
// This is a singleton.
const sessionDataMap = new Map();

/**
 * @typedef {object} SessionDataItem
 * @property {string} moduleName
 *     The name of the module responsible for this data item.
 * @property {SessionDataCategory} category
 *     The category of data. The supported categories depend on the module.
 * @property {(string|number|boolean)} value
 *     Value of the session data item.
 * @property {ContextDescriptor} contextDescriptor
 *     ContextDescriptor to which this session data applies.
 */

/**
 * @typedef SessionDataItemUpdate
 * @property {SessionDataMethod} method
 *     The way sessionData is updated.
 * @property {string} moduleName
 *     The name of the module responsible for this data item.
 * @property {SessionDataCategory} category
 *     The category of data. The supported categories depend on the module.
 * @property {Array<(string|number|boolean)>} values
 *     Values of the session data item update.
 * @property {ContextDescriptor} contextDescriptor
 *     ContextDescriptor to which this session data applies.
 */

/**
 * SessionData provides APIs to read and write the session data for a specific
 * ROOT message handler. It holds the session data as a property and acts as the
 * source of truth for this session data.
 *
 * The session data of a given message handler network should contain all the
 * information that might be needed to setup new contexts, for instance a list
 * of subscribed events, a list of breakpoints etc.
 *
 * The actual session data is an array of SessionDataItems. Example below:
 * ```
 * data: [
 *   {
 *     moduleName: "log",
 *     category: "event",
 *     value: "log.entryAdded",
 *     contextDescriptor: { type: "all" }
 *   },
 *   {
 *     moduleName: "browsingContext",
 *     category: "event",
 *     value: "browsingContext.contextCreated",
 *     contextDescriptor: { type: "browser-element", id: "7"}
 *   },
 *   {
 *     moduleName: "browsingContext",
 *     category: "event",
 *     value: "browsingContext.contextCreated",
 *     contextDescriptor: { type: "browser-element", id: "12"}
 *   },
 * ]
 * ```
 *
 * The session data will be persisted using Services.ppmm.sharedData, so that
 * new contexts living in different processes can also access the information
 * during their startup.
 *
 * This class should only be used from a ROOT MessageHandler, or from modules
 * owned by a ROOT MessageHandler. Other MessageHandlers should rely on
 * SessionDataReader's readSessionData to get read-only access to session data.
 *
 */
export class SessionData {
  constructor(messageHandler) {
    if (messageHandler.constructor.type != lazy.RootMessageHandler.type) {
      throw new Error(
        "SessionData should only be used from a ROOT MessageHandler"
      );
    }

    this._messageHandler = messageHandler;

    /*
     * The actual data for this session. This is an array of SessionDataItems.
     */
    this._data = [];
  }

  destroy() {
    // Update the sessionDataMap singleton.
    sessionDataMap.delete(this._messageHandler.sessionId);

    // Update sharedData and flush to force consistency.
    Services.ppmm.sharedData.set(SESSION_DATA_SHARED_DATA_KEY, sessionDataMap);
    Services.ppmm.sharedData.flush();
  }

  /**
   * Update session data items of a given module, category and
   * contextDescriptor.
   *
   * A SessionDataItem will be added or removed for each value of each update
   * in the provided array.
   *
   * Attempting to add a duplicate SessionDataItem or to remove an unknown
   * SessionDataItem will be silently skipped (no-op).
   *
   * The data will be persisted across processes at the end of this method.
   *
   * @param {Array<SessionDataItemUpdate>} sessionDataItemUpdates
   *     Array of session data item updates.
   *
   * @returns {Array<SessionDataItemUpdate>}
   *     The subset of session data item updates which want to be applied.
   */
  applySessionData(sessionDataItemUpdates = []) {
    // The subset of session data item updates, which are cleaned up from
    // duplicates and unknown items.
    let updates = [];
    for (const sessionDataItemUpdate of sessionDataItemUpdates) {
      const { category, contextDescriptor, method, moduleName, values } =
        sessionDataItemUpdate;
      const updatedValues = [];
      for (const value of values) {
        const item = { moduleName, category, contextDescriptor, value };

        if (method === SessionDataMethod.Add) {
          const hasItem = this._findIndex(item) != -1;

          if (!hasItem) {
            this._data.push(item);
            updatedValues.push(value);
          } else {
            lazy.logger.warn(
              `Duplicated session data item was not added: ${JSON.stringify(
                item
              )}`
            );
          }
        } else {
          const itemIndex = this._findIndex(item);

          if (itemIndex != -1) {
            // The item was found in the session data, remove it.
            this._data.splice(itemIndex, 1);
            updatedValues.push(value);
          } else {
            lazy.logger.warn(
              `Missing session data item was not removed: ${JSON.stringify(
                item
              )}`
            );
          }
        }
      }

      if (updatedValues.length) {
        updates.push({
          ...sessionDataItemUpdate,
          values: updatedValues,
        });
      }
    }
    // Persist the sessionDataMap.
    this._persist();

    return updates;
  }

  /**
   * Retrieve the SessionDataItems for a given module and type.
   *
   * @param {string} moduleName
   *     The name of the module responsible for this data item.
   * @param {string} category
   *     The session data category.
   * @param {ContextDescriptor=} contextDescriptor
   *     Optional context descriptor, to retrieve only session data items added
   *     for a specific context descriptor.
   * @returns {Array<SessionDataItem>}
   *     Array of SessionDataItems for the provided module and type.
   */
  getSessionData(moduleName, category, contextDescriptor) {
    return this._data.filter(
      item =>
        item.moduleName === moduleName &&
        item.category === category &&
        (!contextDescriptor ||
          this._isSameContextDescriptor(
            item.contextDescriptor,
            contextDescriptor
          ))
    );
  }

  /**
   * Update session data items of a given module, category and
   * contextDescriptor and propagate the information
   * via a command to existing MessageHandlers.
   *
   * @param {Array<SessionDataItemUpdate>} sessionDataItemUpdates
   *     Array of session data item updates.
   */
  async updateSessionData(sessionDataItemUpdates = []) {
    const updates = this.applySessionData(sessionDataItemUpdates);

    if (!updates.length) {
      // Avoid unnecessary broadcast if no items were updated.
      return;
    }

    // Create a Map with the structure moduleName -> category -> list of descriptors.
    const structuredUpdates = new Map();
    for (const { moduleName, category, contextDescriptor } of updates) {
      if (!structuredUpdates.has(moduleName)) {
        structuredUpdates.set(moduleName, new Map());
      }
      if (!structuredUpdates.get(moduleName).has(category)) {
        structuredUpdates.get(moduleName).set(category, new Set());
      }
      const descriptors = structuredUpdates.get(moduleName).get(category);
      // If there is at least one update for all contexts,
      // keep only this descriptor in the list of descriptors
      if (contextDescriptor.type === lazy.ContextDescriptorType.All) {
        structuredUpdates
          .get(moduleName)
          .set(category, new Set([contextDescriptor]));
      }
      // Add an individual descriptor if there is no descriptor for all contexts.
      else if (
        descriptors.size !== 1 ||
        Array.from(descriptors)[0]?.type !== lazy.ContextDescriptorType.All
      ) {
        descriptors.add(contextDescriptor);
      }
    }

    const rootDestination = {
      type: lazy.RootMessageHandler.type,
    };
    const sessionDataPromises = [];

    for (const [moduleName, categories] of structuredUpdates.entries()) {
      for (const [category, contextDescriptors] of categories.entries()) {
        // Find sessionData for the category and the moduleName.
        const relevantSessionData = this._data.filter(
          item => item.category == category && item.moduleName === moduleName
        );
        for (const contextDescriptor of contextDescriptors.values()) {
          const windowGlobalDestination = {
            type: lazy.WindowGlobalMessageHandler.type,
            contextDescriptor,
          };

          for (const destination of [
            windowGlobalDestination,
            rootDestination,
          ]) {
            // Only apply session data if the module is present for the destination.
            if (
              this._messageHandler.supportsCommand(
                moduleName,
                "_applySessionData",
                destination
              )
            ) {
              sessionDataPromises.push(
                this._messageHandler
                  .handleCommand({
                    moduleName,
                    commandName: "_applySessionData",
                    params: {
                      sessionData: relevantSessionData,
                      category,
                      contextDescriptor,
                    },
                    destination,
                  })
                  ?.catch(reason =>
                    lazy.logger.error(
                      `_applySessionData for module: ${moduleName} failed, reason: ${reason}`
                    )
                  )
              );
            }
          }
        }
      }
    }

    await Promise.allSettled(sessionDataPromises);
  }

  _isSameItem(item1, item2) {
    const descriptor1 = item1.contextDescriptor;
    const descriptor2 = item2.contextDescriptor;

    return (
      item1.moduleName === item2.moduleName &&
      item1.category === item2.category &&
      this._isSameContextDescriptor(descriptor1, descriptor2) &&
      this._isSameValue(item1.category, item1.value, item2.value)
    );
  }

  _isSameContextDescriptor(contextDescriptor1, contextDescriptor2) {
    if (contextDescriptor1.type === lazy.ContextDescriptorType.All) {
      // Ignore the id for type "all" since we made the id optional for this type.
      return contextDescriptor1.type === contextDescriptor2.type;
    }

    return (
      contextDescriptor1.type === contextDescriptor2.type &&
      contextDescriptor1.id === contextDescriptor2.id
    );
  }

  _isSameValue(category, value1, value2) {
    if (category === SessionDataCategory.PreloadScript) {
      return value1.script === value2.script;
    }

    return value1 === value2;
  }

  _findIndex(item) {
    return this._data.findIndex(_item => this._isSameItem(item, _item));
  }

  _persist() {
    // Update the sessionDataMap singleton.
    sessionDataMap.set(this._messageHandler.sessionId, this._data);

    // Update sharedData and flush to force consistency.
    Services.ppmm.sharedData.set(SESSION_DATA_SHARED_DATA_KEY, sessionDataMap);
    Services.ppmm.sharedData.flush();
  }
}
