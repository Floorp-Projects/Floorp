/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.sys.mjs",
  ExtensionStorageIDB: "resource://gre/modules/ExtensionStorageIDB.sys.mjs",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.sys.mjs",
});

// Wrap a storage operation in a TelemetryStopWatch.
async function measureOp(telemetryMetric, extension, fn) {
  const stopwatchKey = {};
  telemetryMetric.stopwatchStart(extension, stopwatchKey);
  try {
    let result = await fn();
    telemetryMetric.stopwatchFinish(extension, stopwatchKey);
    return result;
  } catch (err) {
    telemetryMetric.stopwatchCancel(extension, stopwatchKey);
    throw err;
  }
}

this.storage = class extends ExtensionAPI {
  getLocalFileBackend(context, { deserialize, serialize }) {
    return {
      get(keys) {
        return measureOp(
          ExtensionTelemetry.storageLocalGetJson,
          context.extension,
          () => {
            return context.childManager
              .callParentAsyncFunction("storage.local.JSONFileBackend.get", [
                serialize(keys),
              ])
              .then(deserialize);
          }
        );
      },
      set(items) {
        return measureOp(
          ExtensionTelemetry.storageLocalSetJson,
          context.extension,
          () => {
            return context.childManager.callParentAsyncFunction(
              "storage.local.JSONFileBackend.set",
              [serialize(items)]
            );
          }
        );
      },
      remove(keys) {
        return context.childManager.callParentAsyncFunction(
          "storage.local.JSONFileBackend.remove",
          [serialize(keys)]
        );
      },
      clear() {
        return context.childManager.callParentAsyncFunction(
          "storage.local.JSONFileBackend.clear",
          []
        );
      },
    };
  }

  getLocalIDBBackend(context, { fireOnChanged, serialize, storagePrincipal }) {
    let dbPromise;
    async function getDB() {
      if (dbPromise) {
        return dbPromise;
      }

      const persisted = context.extension.hasPermission("unlimitedStorage");
      dbPromise = ExtensionStorageIDB.open(storagePrincipal, persisted).catch(
        err => {
          // Reset the cached promise if it has been rejected, so that the next
          // API call is going to retry to open the DB.
          dbPromise = null;
          throw err;
        }
      );

      return dbPromise;
    }

    return {
      get(keys) {
        return measureOp(
          ExtensionTelemetry.storageLocalGetIdb,
          context.extension,
          async () => {
            const db = await getDB();
            return db.get(keys);
          }
        );
      },
      set(items) {
        function serialize(name, anonymizedName, value) {
          return ExtensionStorage.serialize(
            `set/${context.extension.id}/${name}`,
            `set/${context.extension.id}/${anonymizedName}`,
            value
          );
        }

        return measureOp(
          ExtensionTelemetry.storageLocalSetIdb,
          context.extension,
          async () => {
            const db = await getDB();
            const changes = await db.set(items, {
              serialize,
            });

            if (changes) {
              fireOnChanged(changes);
            }
          }
        );
      },
      async remove(keys) {
        const db = await getDB();
        const changes = await db.remove(keys);

        if (changes) {
          fireOnChanged(changes);
        }
      },
      async clear() {
        const db = await getDB();
        const changes = await db.clear(context.extension);

        if (changes) {
          fireOnChanged(changes);
        }
      },
    };
  }

  getAPI(context) {
    const { extension } = context;
    const serialize = ExtensionStorage.serializeForContext.bind(null, context);
    const deserialize = ExtensionStorage.deserializeForContext.bind(
      null,
      context
    );

    // onChangedName is "storage.onChanged", "storage.sync.onChanged", etc.
    function makeOnChangedEventTarget(onChangedName) {
      return new EventManager({
        context,
        name: onChangedName,
        register: fire => {
          let onChanged = (data, area) => {
            let changes = new context.cloneScope.Object();
            for (let [key, value] of Object.entries(data)) {
              changes[key] = deserialize(value);
            }
            if (area) {
              // storage.onChanged includes the area.
              fire.raw(changes, area);
            } else {
              // StorageArea.onChanged doesn't include the area.
              fire.raw(changes);
            }
          };

          let parent = context.childManager.getParentEvent(onChangedName);
          parent.addListener(onChanged);
          return () => {
            parent.removeListener(onChanged);
          };
        },
      }).api();
    }

    function sanitize(items) {
      // The schema validator already takes care of arrays (which are only allowed
      // to contain strings). Strings and null are safe values.
      if (typeof items != "object" || items === null || Array.isArray(items)) {
        return items;
      }
      // If we got here, then `items` is an object generated by `ObjectType`'s
      // `normalize` method from Schemas.jsm. The object returned by `normalize`
      // lives in this compartment, while the values live in compartment of
      // `context.contentWindow`. The `sanitize` method runs with the principal
      // of `context`, so we cannot just use `ExtensionStorage.sanitize` because
      // it is not allowed to access properties of `items`.
      // So we enumerate all properties and sanitize each value individually.
      let sanitized = {};
      for (let [key, value] of Object.entries(items)) {
        sanitized[key] = ExtensionStorage.sanitize(value, context);
      }
      return sanitized;
    }

    function fireOnChanged(changes) {
      // This call is used (by the storage.local API methods for the IndexedDB backend) to fire a storage.onChanged event,
      // it uses the underlying message manager since the child context (or its ProxyContentParent counterpart
      // running in the main process) may be gone by the time we call this, and so we can't use the childManager
      // abstractions (e.g. callParentAsyncFunction or callParentFunctionNoReturn).
      Services.cpmm.sendAsyncMessage(
        `Extension:StorageLocalOnChanged:${extension.uuid}`,
        changes
      );
    }

    // If the selected backend for the extension is not known yet, we have to lazily detect it
    // by asking to the main process (as soon as the storage.local API has been accessed for
    // the first time).
    const getStorageLocalBackend = async () => {
      const { backendEnabled, storagePrincipal } =
        await ExtensionStorageIDB.selectBackend(context);

      if (!backendEnabled) {
        return this.getLocalFileBackend(context, { deserialize, serialize });
      }

      return this.getLocalIDBBackend(context, {
        storagePrincipal,
        fireOnChanged,
        serialize,
      });
    };

    // Synchronously select the backend if it is already known.
    let selectedBackend;

    const useStorageIDBBackend = extension.getSharedData("storageIDBBackend");
    if (useStorageIDBBackend === false) {
      selectedBackend = this.getLocalFileBackend(context, {
        deserialize,
        serialize,
      });
    } else if (useStorageIDBBackend === true) {
      selectedBackend = this.getLocalIDBBackend(context, {
        storagePrincipal: extension.getSharedData("storageIDBPrincipal"),
        fireOnChanged,
        serialize,
      });
    }

    let promiseStorageLocalBackend;

    // Generate the backend-agnostic local API wrapped methods.
    const local = {
      onChanged: makeOnChangedEventTarget("storage.local.onChanged"),
    };
    for (let method of ["get", "set", "remove", "clear"]) {
      local[method] = async function (...args) {
        try {
          // Discover the selected backend if it is not known yet.
          if (!selectedBackend) {
            if (!promiseStorageLocalBackend) {
              promiseStorageLocalBackend = getStorageLocalBackend().catch(
                err => {
                  // Clear the cached promise if it has been rejected.
                  promiseStorageLocalBackend = null;
                  throw err;
                }
              );
            }

            // If the storage.local method is not 'get' (which doesn't change any of the stored data),
            // fall back to call the method in the parent process, so that it can be completed even
            // if this context has been destroyed in the meantime.
            if (method !== "get") {
              // Let the outer try to catch rejections returned by the backend methods.
              try {
                const result =
                  await context.childManager.callParentAsyncFunction(
                    "storage.local.callMethodInParentProcess",
                    [method, args]
                  );
                return result;
              } catch (err) {
                // Just return the rejection as is, the error has been normalized in the
                // parent process by callMethodInParentProcess and the original error
                // logged in the browser console.
                return Promise.reject(err);
              }
            }

            // Get the selected backend and cache it for the next API calls from this context.
            selectedBackend = await promiseStorageLocalBackend;
          }

          // Let the outer try to catch rejections returned by the backend methods.
          const result = await selectedBackend[method](...args);
          return result;
        } catch (err) {
          throw ExtensionStorageIDB.normalizeStorageError({
            error: err,
            extensionId: extension.id,
            storageMethod: method,
          });
        }
      };
    }

    return {
      storage: {
        local,

        session: {
          async get(keys) {
            return deserialize(
              await context.childManager.callParentAsyncFunction(
                "storage.session.get",
                [serialize(keys)]
              )
            );
          },
          set(items) {
            return context.childManager.callParentAsyncFunction(
              "storage.session.set",
              [serialize(items)]
            );
          },
          onChanged: makeOnChangedEventTarget("storage.session.onChanged"),
        },

        sync: {
          get(keys) {
            keys = sanitize(keys);
            return context.childManager.callParentAsyncFunction(
              "storage.sync.get",
              [keys]
            );
          },
          set(items) {
            items = sanitize(items);
            return context.childManager.callParentAsyncFunction(
              "storage.sync.set",
              [items]
            );
          },
          onChanged: makeOnChangedEventTarget("storage.sync.onChanged"),
        },

        managed: {
          get(keys) {
            return context.childManager
              .callParentAsyncFunction("storage.managed.get", [serialize(keys)])
              .then(deserialize);
          },
          set(items) {
            return Promise.reject({ message: "storage.managed is read-only" });
          },
          remove(keys) {
            return Promise.reject({ message: "storage.managed is read-only" });
          },
          clear() {
            return Promise.reject({ message: "storage.managed is read-only" });
          },

          onChanged: makeOnChangedEventTarget("storage.managed.onChanged"),
        },

        onChanged: makeOnChangedEventTarget("storage.onChanged"),
      },
    };
  }
};
