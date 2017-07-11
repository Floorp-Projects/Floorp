/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionStorage"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

const global = this;

function isStructuredCloneHolder(value) {
  return (value && typeof value === "object" &&
          Cu.getClassName(value, true) === "StructuredCloneHolder");
}

class SerializeableMap extends Map {
  toJSON() {
    let result = {};
    for (let [key, value] of this) {
      if (isStructuredCloneHolder(value)) {
        value = value.deserialize(global);
        this.set(key, value);
      }

      result[key] = value;
    }
    return result;
  }

  /**
   * Like toJSON, but attempts to serialize every value separately, and
   * elides any which fail to serialize. Should only be used if initial
   * JSON serialization fails.
   *
   * @returns {object}
   */
  toJSONSafe() {
    let result = {};
    for (let [key, value] of this) {
      try {
        void JSON.serialize(value);

        result[key] = value;
      } catch (e) {
        Cu.reportError(new Error(`Failed to serialize browser.storage key "${key}": ${e}`));
      }
    }
    return result;
  }
}

/**
 * Serializes an arbitrary value into a StructuredCloneHolder, if
 * appropriate. Existing StructuredCloneHolders are returned unchanged.
 * Non-object values are also returned unchanged. Anything else is
 * serialized, and a new StructuredCloneHolder returned.
 *
 * This allows us to avoid a second structured clone operation after
 * sending a storage value across a message manager, before cloning it
 * into an extension scope.
 *
 * @param {StructuredCloneHolder|*} value
 *        A value to serialize.
 * @returns {*}
 */
function serialize(value) {
  if (value && typeof value === "object" && !isStructuredCloneHolder(value)) {
    return new StructuredCloneHolder(value);
  }
  return value;
}

this.ExtensionStorage = {
  // Map<extension-id, Promise<JSONFile>>
  jsonFilePromises: new Map(),

  listeners: new Map(),

  /**
   * Asynchronously reads the storage file for the given extension ID
   * and returns a Promise for its initialized JSONFile object.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to return a file.
   * @returns {Promise<JSONFile>}
   */
  async _readFile(extensionId) {
    OS.File.makeDir(this.getExtensionDir(extensionId), {
      ignoreExisting: true,
      from: OS.Constants.Path.profileDir,
    });

    let jsonFile = new JSONFile({path: this.getStorageFile(extensionId)});
    await jsonFile.load();

    jsonFile.data = new SerializeableMap(Object.entries(jsonFile.data));

    return jsonFile;
  },

  /**
   * Returns a Promise for initialized JSONFile instance for the
   * extension's storage file.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to return a file.
   * @returns {Promise<JSONFile>}
   */
  getFile(extensionId) {
    let promise = this.jsonFilePromises.get(extensionId);
    if (!promise) {
      promise = this._readFile(extensionId);
      this.jsonFilePromises.set(extensionId, promise);
    }
    return promise;
  },

  /**
   * Sanitizes the given value, and returns a JSON-compatible
   * representation of it, based on the privileges of the given global.
   *
   * @param {value} value
   *        The value to sanitize.
   * @param {Context} context
   *        The extension context in which to sanitize the value
   * @returns {value}
   *        The sanitized value.
   */
  sanitize(value, context) {
    let json = context.jsonStringify(value === undefined ? null : value);
    if (json == undefined) {
      throw new ExtensionUtils.ExtensionError("DataCloneError: The object could not be cloned.");
    }
    return JSON.parse(json);
  },


  /**
   * Returns the path to the storage directory within the profile for
   * the given extension ID.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to return a directory path.
   * @returns {string}
   */
  getExtensionDir(extensionId) {
    return OS.Path.join(this.extensionDir, extensionId);
  },

  /**
   * Returns the path to the JSON storage file for the given extension
   * ID.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to return a file path.
   * @returns {string}
   */
  getStorageFile(extensionId) {
    return OS.Path.join(this.extensionDir, extensionId, "storage.js");
  },

  /**
   * Asynchronously sets the values of the given storage items for the
   * given extension.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to set storage values.
   * @param {object} items
   *        The storage items to set. For each property in the object,
   *        the storage value for that property is set to its value in
   *        said object. Any values which are StructuredCloneHolder
   *        instances are deserialized before being stored.
   * @returns {Promise<void>}
   */
  async set(extensionId, items) {
    let jsonFile = await this.getFile(extensionId);

    let changes = {};
    for (let prop in items) {
      let item = items[prop];
      changes[prop] = {oldValue: serialize(jsonFile.data.get(prop)), newValue: serialize(item)};
      jsonFile.data.set(prop, item);
    }

    this.notifyListeners(extensionId, changes);

    jsonFile.saveSoon();
    return null;
  },

  /**
   * Asynchronously removes the given storage items for the given
   * extension ID.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to remove storage values.
   * @param {Array<string>} items
   *        A list of storage items to remove.
   * @returns {Promise<void>}
   */
  async remove(extensionId, items) {
    let jsonFile = await this.getFile(extensionId);

    let changed = false;
    let changes = {};

    for (let prop of [].concat(items)) {
      if (jsonFile.data.has(prop)) {
        changes[prop] = {oldValue: serialize(jsonFile.data.get(prop))};
        jsonFile.data.delete(prop);
        changed = true;
      }
    }

    if (changed) {
      this.notifyListeners(extensionId, changes);
      jsonFile.saveSoon();
    }
    return null;
  },

  /**
   * Asynchronously clears all storage entries for the given extension
   * ID.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to clear storage.
   * @returns {Promise<void>}
   */
  async clear(extensionId) {
    let jsonFile = await this.getFile(extensionId);

    let changed = false;
    let changes = {};

    for (let [prop, oldValue] of jsonFile.data.entries()) {
      changes[prop] = {oldValue: serialize(oldValue)};
      jsonFile.data.delete(prop);
      changed = true;
    }

    if (changed) {
      this.notifyListeners(extensionId, changes);
      jsonFile.saveSoon();
    }
    return null;
  },

  /**
   * Asynchronously retrieves the values for the given storage items for
   * the given extension ID.
   *
   * @param {string} extensionId
   *        The ID of the extension for which to get storage values.
   * @param {Array<string>|object|null} [keys]
   *        The storage items to get. If an array, the value of each key
   *        in the array is returned. If null, the values of all items
   *        are returned. If an object, the value for each key in the
   *        object is returned, or that key's value if the item is not
   *        set.
   * @returns {Promise<object>}
   *        An object which a property for each requested key,
   *        containing that key's storage value. Values are
   *        StructuredCloneHolder objects which can be deserialized to
   *        the original storage value.
   */
  async get(extensionId, keys) {
    let jsonFile = await this.getFile(extensionId);
    let {data} = jsonFile;

    let result = {};
    if (keys === null) {
      Object.assign(result, data.toJSON());
    } else if (typeof(keys) == "object" && !Array.isArray(keys)) {
      for (let prop in keys) {
        if (data.has(prop)) {
          result[prop] = serialize(data.get(prop));
        } else {
          result[prop] = keys[prop];
        }
      }
    } else {
      for (let prop of [].concat(keys)) {
        if (data.has(prop)) {
          result[prop] = serialize(data.get(prop));
        }
      }
    }

    return result;
  },

  addOnChangedListener(extensionId, listener) {
    let listeners = this.listeners.get(extensionId) || new Set();
    listeners.add(listener);
    this.listeners.set(extensionId, listeners);
  },

  removeOnChangedListener(extensionId, listener) {
    let listeners = this.listeners.get(extensionId);
    listeners.delete(listener);
  },

  notifyListeners(extensionId, changes) {
    let listeners = this.listeners.get(extensionId);
    if (listeners) {
      for (let listener of listeners) {
        listener(changes);
      }
    }
  },

  init() {
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      return;
    }
    Services.obs.addObserver(this, "extension-invalidate-storage-cache");
    Services.obs.addObserver(this, "xpcom-shutdown");
  },

  observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "extension-invalidate-storage-cache");
      Services.obs.removeObserver(this, "xpcom-shutdown");
    } else if (topic == "extension-invalidate-storage-cache") {
      for (let promise of this.jsonFilePromises.values()) {
        promise.then(jsonFile => { jsonFile.finalize(); });
      }
      this.jsonFilePromises.clear();
    }
  },
};

XPCOMUtils.defineLazyGetter(ExtensionStorage, "extensionDir",
  () => OS.Path.join(OS.Constants.Path.profileDir, "browser-extension-data"));

ExtensionStorage.init();
