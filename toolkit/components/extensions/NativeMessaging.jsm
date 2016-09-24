/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HostManifestManager", "NativeApp"];
/* globals NativeApp */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {EventEmitter} = Cu.import("resource://devtools/shared/event-emitter.js", {});

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Subprocess",
                                  "resource://gre/modules/Subprocess.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");

const HOST_MANIFEST_SCHEMA = "chrome://extensions/content/schemas/native_host_manifest.json";
const VALID_APPLICATION = /^\w+(\.\w+)*$/;

// For a graceful shutdown (i.e., when the extension is unloaded or when it
// explicitly calls disconnect() on a native port), how long we give the native
// application to exit before we start trying to kill it.  (in milliseconds)
const GRACEFUL_SHUTDOWN_TIME = 3000;

// Hard limits on maximum message size that can be read/written
// These are defined in the native messaging documentation, note that
// the write limit is imposed by the "wire protocol" in which message
// boundaries are defined by preceding each message with its length as
// 4-byte unsigned integer so this is the largest value that can be
// represented.  Good luck generating a serialized message that large,
// the practical write limit is likely to be dictated by available memory.
const MAX_READ = 1024 * 1024;
const MAX_WRITE = 0xffffffff;

// Preferences that can lower the message size limits above,
// used for testing the limits.
const PREF_MAX_READ = "webextensions.native-messaging.max-input-message-bytes";
const PREF_MAX_WRITE = "webextensions.native-messaging.max-output-message-bytes";

const REGPATH = "Software\\Mozilla\\NativeMessagingHosts";

this.HostManifestManager = {
  _initializePromise: null,
  _lookup: null,

  init() {
    if (!this._initializePromise) {
      let platform = AppConstants.platform;
      if (platform == "win") {
        this._lookup = this._winLookup;
      } else if (platform == "macosx" || platform == "linux") {
        let dirs = [
          Services.dirsvc.get("XREUserNativeMessaging", Ci.nsIFile).path,
          Services.dirsvc.get("XRESysNativeMessaging", Ci.nsIFile).path,
        ];
        this._lookup = (application, context) => this._tryPaths(application, dirs, context);
      } else {
        throw new Error(`Native messaging is not supported on ${AppConstants.platform}`);
      }
      this._initializePromise = Schemas.load(HOST_MANIFEST_SCHEMA);
    }
    return this._initializePromise;
  },

  _winLookup(application, context) {
    const REGISTRY = Ci.nsIWindowsRegKey;
    let regPath = `${REGPATH}\\${application}`;
    let path = WindowsRegistry.readRegKey(REGISTRY.ROOT_KEY_CURRENT_USER,
                                          regPath, "", REGISTRY.WOW64_64);
    if (!path) {
      path = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                        regPath, "", REGISTRY.WOW64_64);
    }
    if (!path) {
      return null;
    }
    return this._tryPath(path, application, context)
      .then(manifest => manifest ? {path, manifest} : null);
  },

  _tryPath(path, application, context) {
    return Promise.resolve()
      .then(() => OS.File.read(path, {encoding: "utf-8"}))
      .then(data => {
        let manifest;
        try {
          manifest = JSON.parse(data);
        } catch (ex) {
          let msg = `Error parsing native host manifest ${path}: ${ex.message}`;
          Cu.reportError(msg);
          return null;
        }

        let normalized = Schemas.normalize(manifest, "manifest.NativeHostManifest", context);
        if (normalized.error) {
          Cu.reportError(normalized.error);
          return null;
        }
        manifest = normalized.value;
        if (manifest.name != application) {
          let msg = `Native host manifest ${path} has name property ${manifest.name} (expected ${application})`;
          Cu.reportError(msg);
          return null;
        }
        return normalized.value;
      }).catch(ex => {
        if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          return null;
        }
        throw ex;
      });
  },

  _tryPaths: Task.async(function* (application, dirs, context) {
    for (let dir of dirs) {
      let path = OS.Path.join(dir, `${application}.json`);
      let manifest = yield this._tryPath(path, application, context);
      if (manifest) {
        return {path, manifest};
      }
    }
    return null;
  }),

  /**
   * Search for a valid native host manifest for the given application name.
   * The directories searched and rules for manifest validation are all
   * detailed in the native messaging documentation.
   *
   * @param {string} application The name of the applciation to search for.
   * @param {object} context A context object as expected by Schemas.normalize.
   * @returns {object} The contents of the validated manifest, or null if
   *                   no valid manifest can be found for this application.
   */
  lookupApplication(application, context) {
    if (!VALID_APPLICATION.test(application)) {
      throw new Error(`Invalid application "${application}"`);
    }
    return this.init().then(() => this._lookup(application, context));
  },
};

this.NativeApp = class extends EventEmitter {
  constructor(context, application) {
    super();

    this.context = context;
    this.name = application;

    // We want a close() notification when the window is destroyed.
    this.context.callOnClose(this);

    this.proc = null;
    this.readPromise = null;
    this.sendQueue = [];
    this.writePromise = null;
    this.sentDisconnect = false;

    this.startupPromise = HostManifestManager.lookupApplication(application, context)
      .then(hostInfo => {
        if (!hostInfo) {
          throw new Error(`No such native application ${application}`);
        }

        if (!hostInfo.manifest.allowed_extensions.includes(context.extension.id)) {
          throw new Error(`This extension does not have permission to use native application ${application}`);
        }

        let command = hostInfo.manifest.path;
        if (AppConstants.platform == "win") {
          // OS.Path.join() ignores anything before the last absolute path
          // it sees, so if command is already absolute, it remains unchanged
          // here.  If it is relative, we get the proper absolute path here.
          command = OS.Path.join(OS.Path.dirname(hostInfo.path), command);
        }

        let subprocessOpts = {
          command: command,
          arguments: [hostInfo.path],
          workdir: OS.Path.dirname(command),
          stderr: "pipe",
        };
        return Subprocess.call(subprocessOpts);
      }).then(proc => {
        this.startupPromise = null;
        this.proc = proc;
        this._startRead();
        this._startWrite();
        this._startStderrRead();
      }).catch(err => {
        this.startupPromise = null;
        Cu.reportError(err instanceof Error ? err : err.message);
        this._cleanup(err);
      });
  }

  /**
   * @param {BaseContext} context The scope from where `message` originates.
   * @param {*} message A message from the extension, meant for a native app.
   * @returns {ArrayBuffer} An ArrayBuffer that can be sent to the native app.
   */
  static encodeMessage(context, message) {
    message = context.jsonStringify(message);
    let buffer = new TextEncoder().encode(message).buffer;
    if (buffer.byteLength > NativeApp.maxWrite) {
      throw new context.cloneScope.Error("Write too big");
    }
    return buffer;
  }

  // A port is definitely "alive" if this.proc is non-null.  But we have
  // to provide a live port object immediately when connecting so we also
  // need to consider a port alive if proc is null but the startupPromise
  // is still pending.
  get _isDisconnected() {
    return (!this.proc && !this.startupPromise);
  }

  _startRead() {
    if (this.readPromise) {
      throw new Error("Entered _startRead() while readPromise is non-null");
    }
    this.readPromise = this.proc.stdout.readUint32()
      .then(len => {
        if (len > NativeApp.maxRead) {
          throw new Error(`Native application tried to send a message of ${len} bytes, which exceeds the limit of ${NativeApp.maxRead} bytes.`);
        }
        return this.proc.stdout.readJSON(len);
      }).then(msg => {
        this.emit("message", msg);
        this.readPromise = null;
        this._startRead();
      }).catch(err => {
        if (err.errorCode != Subprocess.ERROR_END_OF_FILE) {
          Cu.reportError(err instanceof Error ? err : err.message);
        }
        this._cleanup(err);
      });
  }

  _startWrite() {
    if (this.sendQueue.length == 0) {
      return;
    }

    if (this.writePromise) {
      throw new Error("Entered _startWrite() while writePromise is non-null");
    }

    let buffer = this.sendQueue.shift();
    let uintArray = Uint32Array.of(buffer.byteLength);

    this.writePromise = Promise.all([
      this.proc.stdin.write(uintArray.buffer),
      this.proc.stdin.write(buffer),
    ]).then(() => {
      this.writePromise = null;
      this._startWrite();
    }).catch(err => {
      Cu.reportError(err.message);
      this._cleanup(err);
    });
  }

  _startStderrRead() {
    let proc = this.proc;
    let app = this.name;
    Task.spawn(function* () {
      let partial = "";
      while (true) {
        let data = yield proc.stderr.readString();
        if (data.length == 0) {
          // We have hit EOF, just stop reading
          if (partial) {
            Services.console.logStringMessage(`stderr output from native app ${app}: ${partial}`);
          }
          break;
        }

        let lines = data.split(/\r?\n/);
        lines[0] = partial + lines[0];
        partial = lines.pop();

        for (let line of lines) {
          Services.console.logStringMessage(`stderr output from native app ${app}: ${line}`);
        }
      }
    });
  }

  send(msg) {
    if (this._isDisconnected) {
      throw new this.context.cloneScope.Error("Attempt to postMessage on disconnected port");
    }
    if (Cu.getClassName(msg, true) != "ArrayBuffer") {
      // This error cannot be triggered by extensions; it indicates an error in
      // our implementation.
      throw new Error("The message to the native messaging host is not an ArrayBuffer");
    }

    let buffer = msg;

    if (buffer.byteLength > NativeApp.maxWrite) {
      throw new this.context.cloneScope.Error("Write too big");
    }

    this.sendQueue.push(buffer);
    if (!this.startupPromise && !this.writePromise) {
      this._startWrite();
    }
  }

  // Shut down the native application and also signal to the extension
  // that the connect has been disconnected.
  _cleanup(err) {
    this.context.forgetOnClose(this);

    let doCleanup = () => {
      // Set a timer to kill the process gracefully after one timeout
      // interval and kill it forcefully after two intervals.
      let timer = setTimeout(() => {
        this.proc.kill(GRACEFUL_SHUTDOWN_TIME);
      }, GRACEFUL_SHUTDOWN_TIME);

      let promise = Promise.all([
        this.proc.stdin.close()
          .catch(err => {
            if (err.errorCode != Subprocess.ERROR_END_OF_FILE) {
              throw err;
            }
          }),
        this.proc.wait(),
      ]).then(() => {
        this.proc = null;
        clearTimeout(timer);
      });

      AsyncShutdown.profileBeforeChange.addBlocker(
        `Native Messaging: Wait for application ${this.name} to exit`,
        promise);

      promise.then(() => {
        AsyncShutdown.profileBeforeChange.removeBlocker(promise);
      });

      return promise;
    };

    if (this.proc) {
      doCleanup();
    } else if (this.startupPromise) {
      this.startupPromise.then(doCleanup);
    }

    if (!this.sentDisconnect) {
      this.sentDisconnect = true;
      this.emit("disconnect", err);
    }
  }

  // Called from Context when the extension is shut down.
  close() {
    this._cleanup();
  }

  portAPI() {
    let port = {
      name: this.name,

      disconnect: () => {
        if (this._isDisconnected) {
          throw new this.context.cloneScope.Error("Attempt to disconnect an already disconnected port");
        }
        this._cleanup();
      },

      postMessage: msg => {
        msg = NativeApp.encodeMessage(this.context, msg);
        this.send(msg);
      },

      onDisconnect: new ExtensionUtils.SingletonEventManager(this.context, "native.onDisconnect", fire => {
        let listener = what => {
          this.context.runSafeWithoutClone(fire, port);
        };
        this.on("disconnect", listener);
        return () => {
          this.off("disconnect", listener);
        };
      }).api(),

      onMessage: new ExtensionUtils.SingletonEventManager(this.context, "native.onMessage", fire => {
        let listener = (what, msg) => {
          msg = Cu.cloneInto(msg, this.context.cloneScope);
          this.context.runSafeWithoutClone(fire, msg, port);
        };
        this.on("message", listener);
        return () => {
          this.off("message", listener);
        };
      }).api(),
    };

    port = Cu.cloneInto(port, this.context.cloneScope, {cloneFunctions: true});

    return port;
  }

  sendMessage(msg) {
    let responsePromise = new Promise((resolve, reject) => {
      this.on("message", (what, msg) => { resolve(msg); });
      this.on("disconnect", (what, err) => { reject(err); });
    });

    let result = this.startupPromise.then(() => {
      this.send(msg);
      return responsePromise;
    });

    result.then(() => {
      this._cleanup();
    }, () => {
      // Prevent the response promise from being reported as an
      // unchecked rejection if the startup promise fails.
      responsePromise.catch(() => {});

      this._cleanup();
    });

    return result;
  }
};

XPCOMUtils.defineLazyPreferenceGetter(NativeApp, "maxRead", PREF_MAX_READ, MAX_READ);
XPCOMUtils.defineLazyPreferenceGetter(NativeApp, "maxWrite", PREF_MAX_WRITE, MAX_WRITE);
