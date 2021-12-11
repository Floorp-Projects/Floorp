/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* eslint-disable mozilla/balanced-listeners */

var EXPORTED_SYMBOLS = ["SubprocessImpl"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { ctypes } = ChromeUtils.import("resource://gre/modules/ctypes.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { BaseProcess, PromiseWorker } = ChromeUtils.import(
  "resource://gre/modules/subprocess/subprocess_common.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "env",
  "@mozilla.org/process/environment;1",
  "nsIEnvironment"
);

/* import-globals-from subprocess_shared.js */
/* import-globals-from subprocess_shared_win.js */
Services.scriptloader.loadSubScript(
  "resource://gre/modules/subprocess/subprocess_shared.js",
  this
);
Services.scriptloader.loadSubScript(
  "resource://gre/modules/subprocess/subprocess_shared_win.js",
  this
);

class WinPromiseWorker extends PromiseWorker {
  constructor(...args) {
    super(...args);

    this.signalEvent = libc.CreateSemaphoreW(null, 0, 32, null);

    this.call("init", [
      {
        breakAwayFromJob: !AppConstants.isPlatformAndVersionAtLeast(
          "win",
          "6.2"
        ),
        comspec: env.get("COMSPEC"),
        signalEvent: String(
          ctypes.cast(this.signalEvent, ctypes.uintptr_t).value
        ),
      },
    ]);
  }

  signalWorker() {
    libc.ReleaseSemaphore(this.signalEvent, 1, null);
  }

  postMessage(...args) {
    this.signalWorker();
    return super.postMessage(...args);
  }
}

class Process extends BaseProcess {
  static get WORKER_URL() {
    return "resource://gre/modules/subprocess/subprocess_worker_win.js";
  }

  static get WorkerClass() {
    return WinPromiseWorker;
  }
}

var SubprocessWin = {
  Process,

  call(options) {
    return Process.create(options);
  },

  *getEnvironment() {
    let env = libc.GetEnvironmentStringsW();
    try {
      for (let p = env, q = env; ; p = p.increment()) {
        if (p.contents == "\0") {
          if (String(p) == String(q)) {
            break;
          }

          let str = q.readString();
          q = p.increment();

          let idx = str.indexOf("=");
          if (idx == 0) {
            idx = str.indexOf("=", 1);
          }

          if (idx >= 0) {
            yield [str.slice(0, idx), str.slice(idx + 1)];
          }
        }
      }
    } finally {
      libc.FreeEnvironmentStringsW(env);
    }
  },

  async isExecutableFile(path) {
    if (!OS.Path.split(path).absolute) {
      return false;
    }

    try {
      let info = await OS.File.stat(path);
      return !(info.isDir || info.isSymlink);
    } catch (e) {
      return false;
    }
  },

  /**
   * Searches for the given executable file in the system executable
   * file paths as specified by the PATH environment variable.
   *
   * On Windows, if the unadorned filename cannot be found, the
   * extensions in the semicolon-separated list in the PATHEXT
   * environment variable are successively appended to the original
   * name and searched for in turn.
   *
   * @param {string} bin
   *        The name of the executable to find.
   * @param {object} environment
   *        An object containing a key for each environment variable to be used
   *        in the search.
   * @returns {Promise<string>}
   */
  async pathSearch(bin, environment) {
    let split = OS.Path.split(bin);
    if (split.absolute) {
      if (await this.isExecutableFile(bin)) {
        return bin;
      }
      let error = new Error(
        `File at path "${bin}" does not exist, or is not a normal file`
      );
      error.errorCode = SubprocessConstants.ERROR_BAD_EXECUTABLE;
      throw error;
    }

    let dirs = [];
    let exts = [];
    if (environment.PATH) {
      dirs = environment.PATH.split(";");
    }
    if (environment.PATHEXT) {
      exts = environment.PATHEXT.split(";");
    }

    for (let dir of dirs) {
      let path = OS.Path.join(dir, bin);

      if (await this.isExecutableFile(path)) {
        return path;
      }

      for (let ext of exts) {
        let file = path + ext;

        if (await this.isExecutableFile(file)) {
          return file;
        }
      }
    }
    let error = new Error(`Executable not found: ${bin}`);
    error.errorCode = SubprocessConstants.ERROR_BAD_EXECUTABLE;
    throw error;
  },
};

var SubprocessImpl = SubprocessWin;
