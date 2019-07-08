/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionStorage"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionUtils",
  "resource://gre/modules/ExtensionUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

const global = this;

function isStructuredCloneHolder(value) {
  return (
    value &&
    typeof value === "object" &&
    Cu.getClassName(value, true) === "StructuredCloneHolder"
  );
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
        void JSON.stringify(value);

        result[key] = value;
      } catch (e) {
        Cu.reportError(
          new Error(`Failed to serialize browser.storage key "${key}": ${e}`)
        );
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

var ExtensionStorage = {
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

    let jsonFile = new JSONFile({ path: this.getStorageFile(extensionId) });
    await jsonFile.load();

    jsonFile.data = this._serializableMap(jsonFile.data);
    return jsonFile;
  },

  _serializableMap(data) {
    return new SerializeableMap(Object.entries(data));
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
   * Clear the cached jsonFilePromise for a given extensionId
   * (used by ExtensionStorageIDB to free the jsonFile once the data migration
   * has been completed).
   *
   * @param {string} extensionId
   *        The ID of the extension for which to return a file.
   */
  async clearCachedFile(extensionId) {
    let promise = this.jsonFilePromises.get(extensionId);
    if (promise) {
      this.jsonFilePromises.delete(extensionId);
      await promise.then(jsonFile => jsonFile.finalize());
    }
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
      throw new ExtensionUtils.ExtensionError(
        "DataCloneError: The object could not be cloned."
      );
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
      changes[prop] = {
        oldValue: serialize(jsonFile.data.get(prop)),
        newValue: serialize(item),
      };
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
        changes[prop] = { oldValue: serialize(jsonFile.data.get(prop)) };
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
   * @param {object} options
   * @param {boolean} [options.shouldNotifyListeners = true]
   *         Whether or not collect and send the changes to the listeners,
   *         used when the extension data is being cleared on uninstall.
   * @returns {Promise<void>}
   */
  async clear(extensionId, { shouldNotifyListeners = true } = {}) {
    let jsonFile = await this.getFile(extensionId);

    let changed = false;
    let changes = {};

    for (let [prop, oldValue] of jsonFile.data.entries()) {
      if (shouldNotifyListeners) {
        changes[prop] = { oldValue: serialize(oldValue) };
      }

      jsonFile.data.delete(prop);
      changed = true;
    }

    if (changed) {
      if (shouldNotifyListeners) {
        this.notifyListeners(extensionId, changes);
      }

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
    return this._filterProperties(jsonFile.data, keys);
  },

  async _filterProperties(data, keys) {
    let result = {};
    if (keys === null) {
      Object.assign(result, data.toJSON());
    } else if (typeof keys == "object" && !Array.isArray(keys)) {
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
        promise.then(jsonFile => {
          jsonFile.finalize();
        });
      }
      this.jsonFilePromises.clear();
    }
  },

  // Serializes an arbitrary value into a StructuredCloneHolder, if appropriate.
  serialize,

  /**
   * Serializes the given storage items for transporting between processes.
   *
   * @param {BaseContext} context
   *        The context to use for the created StructuredCloneHolder
   *        objects.
   * @param {Array<string>|object} items
   *        The items to serialize. If an object is provided, its
   *        values are serialized to StructuredCloneHolder objects.
   *        Otherwise, it is returned as-is.
   * @returns {Array<string>|object}
   */
  serializeForContext(context, items) {
    if (items && typeof items === "object" && !Array.isArray(items)) {
      let result = {};
      for (let [key, value] of Object.entries(items)) {
        try {
          result[key] = new StructuredCloneHolder(value, context.cloneScope);
        } catch (e) {
          throw new ExtensionUtils.ExtensionError(String(e));
        }
      }
      return result;
    }
    return items;
  },

  /**
   * Deserializes the given storage items into the given extension context.
   *
   * @param {BaseContext} context
   *        The context to use to deserialize the StructuredCloneHolder objects.
   * @param {object} items
   *        The items to deserialize. Any property of the object which
   *        is a StructuredCloneHolder instance is deserialized into
   *        the extension scope. Any other object is cloned into the
   *        extension scope directly.
   * @returns {object}
   */
  deserializeForContext(context, items) {
    let result = new context.cloneScope.Object();
    for (let [key, value] of Object.entries(items)) {
      if (
        value &&
        typeof value === "object" &&
        Cu.getClassName(value, true) === "StructuredCloneHolder"
      ) {
        value = value.deserialize(context.cloneScope, true);
      } else {
        value = Cu.cloneInto(value, context.cloneScope);
      }
      result[key] = value;
    }
    return result;
  },
};

XPCOMUtils.defineLazyGetter(ExtensionStorage, "extensionDir", () =>
  OS.Path.join(OS.Constants.Path.profileDir, "browser-extension-data")
);

ExtensionStorage.init();
