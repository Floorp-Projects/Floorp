/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SESSION_DATA_SHARED_DATA_KEY", "SessionData"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

const SESSION_DATA_SHARED_DATA_KEY = "MessageHandlerSessionData";

// This is a map from session id to session data, which will be persisted and
// propagated to all processes using Services' sharedData.
// We have to store this as a unique object under a unique shared data key
// because new MessageHandlers in other processes will need to access this data
// without any notion of a specific session.
// This is a singleton.
const sessionDataMap = new Map();

/**
 * @typedef {Object} SessionDataItem
 * @property {String} moduleName
 *     The name of the module responsible for this data item.
 * @property {String} category
 *     The category of data. The supported categories depend on the module.
 * @property {(string|number|boolean)} value
 *     Value of the session data item.
 * @property {ContextDescriptor} contextDescriptor
 *     ContextDescriptor to which this session data applies
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
class SessionData {
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

  /**
   * Add new session data items of a given module, category and
   * contextDescriptor.
   *
   * A new SessionDataItem will be created for each value of the values array.
   *
   * If a SessionDataItem already exists for the provided value, moduleName,
   * category and contextDescriptor, it will be skipped to avoid duplicated
   * SessionDataItems.
   *
   * The data will be persisted across processes at the end of this method.
   *
   * @param {String} moduleName
   *     The name of the module responsible for this data item.
   * @param {String} category
   *     The session data category.
   * @param {ContextDescriptor} contextDescriptor
   *     The contextDescriptor object defining the scope of the session data
   *     values.
   * @param {Array<(string|number|boolean)>} values
   *     Array of session data item values.
   * @return {Array<(string|number|boolean)>}
   *     The subset of values actually added to the session data.
   */
  addSessionData(moduleName, category, contextDescriptor, values) {
    const addedValues = [];
    for (const value of values) {
      const item = { moduleName, category, contextDescriptor, value };

      const hasItem = this._data.some(_item => this._isSameItem(item, _item));
      if (!hasItem) {
        // This is a new data item, create it and add it to the data.
        this._data.push(item);
        addedValues.push(value);
      } else {
        lazy.logger.warn(
          `Duplicated session data item was not added: ${JSON.stringify(item)}`
        );
      }
    }

    // Persist the sessionDataMap.
    this._persist();

    return addedValues;
  }

  destroy() {
    // Update the sessionDataMap singleton.
    sessionDataMap.delete(this._messageHandler.sessionId);

    // Update sharedData and flush to force consistency.
    Services.ppmm.sharedData.set(SESSION_DATA_SHARED_DATA_KEY, sessionDataMap);
    Services.ppmm.sharedData.flush();
  }

  /**
   * Retrieve the SessionDataItems for a given module and type.
   *
   * @param {String} moduleName
   *     The name of the module responsible for this data item.
   * @param {String} category
   *     The session data category.
   * @param {ContextDescriptor=} contextDescriptor
   *     Optional context descriptor, to retrieve only session data items added
   *     for a specific context descriptor.
   * @return {Array<SessionDataItem>}
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
   * Remove values for the provided module, category and context.
   * Values which don't match any existing SessionDataItem will be ignored.
   *
   * The updated sessionDataMap will be persisted across processes at the end.
   *
   * @param {String} moduleName
   *     The name of the module responsible for this data item.
   * @param {String} category
   *     The session data category.
   * @param {ContextDescriptor} contextDescriptor
   *     The contextDescriptor object defining the scope of the session data
   *     values.
   * @param {Array<(string|number|boolean)>} values
   *     Array of session data item values.
   * @return {Array<(string|number|boolean)>}
   *     The subset of values actually removed from the session data.
   */
  removeSessionData(moduleName, category, contextDescriptor, values) {
    const removedValues = [];
    // Remove the provided context from the contexts Map of the provided items.
    for (const value of values) {
      const item = { moduleName, category, contextDescriptor, value };

      const itemIndex = this._data.findIndex(_item =>
        this._isSameItem(item, _item)
      );
      if (itemIndex != -1) {
        // The item was found in the session data, remove it.
        this._data.splice(itemIndex, 1);
        removedValues.push(value);
      } else {
        lazy.logger.warn(
          `Missing session data item was not removed: ${JSON.stringify(item)}`
        );
      }
    }

    // Persist the sessionDataMap.
    this._persist();

    return removedValues;
  }

  updateSessionData(moduleName, category, contextDescriptor, added, removed) {
    this.addSessionData(moduleName, category, contextDescriptor, added);
    this.removeSessionData(moduleName, category, contextDescriptor, removed);
  }

  _isSameItem(item1, item2) {
    const descriptor1 = item1.contextDescriptor;
    const descriptor2 = item2.contextDescriptor;

    return (
      item1.moduleName === item2.moduleName &&
      item1.category === item2.category &&
      this._isSameContextDescriptor(descriptor1, descriptor2) &&
      item1.value === item2.value
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

  _persist() {
    // Update the sessionDataMap singleton.
    sessionDataMap.set(this._messageHandler.sessionId, this._data);

    // Update sharedData and flush to force consistency.
    Services.ppmm.sharedData.set(SESSION_DATA_SHARED_DATA_KEY, sessionDataMap);
    Services.ppmm.sharedData.flush();
  }
}
