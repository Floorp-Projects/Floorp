/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * These modules are loosely based on the subprocess.jsm module created
 * by Jan Gerber and Patrick Brunschwig, though the implementation
 * differs drastically.
 */

"use strict";

var EXPORTED_SYMBOLS = ["Subprocess"];

/* exported Subprocess */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["TextEncoder"]);

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/subprocess/subprocess_common.jsm");

if (AppConstants.platform == "win") {
  XPCOMUtils.defineLazyModuleGetter(this, "SubprocessImpl",
                                    "resource://gre/modules/subprocess/subprocess_win.jsm");
} else {
  XPCOMUtils.defineLazyModuleGetter(this, "SubprocessImpl",
                                    "resource://gre/modules/subprocess/subprocess_unix.jsm");
}

function encodeEnvVar(name, value) {
  if (typeof name === "string" && typeof value === "string") {
    return `${name}=${value}`;
  }

  let encoder = new TextEncoder("utf-8");
  function encode(val) {
    return typeof val === "string" ? encoder.encode(val) : val;
  }

  return Uint8Array.of(...encode(name), ...encode("="), ...encode(value), 0);
}

/**
 * Allows for creation of and communication with OS-level sub-processes.
 * @namespace
 */
var Subprocess = {
  /**
   * Launches a process, and returns a handle to it.
   *
   * @param {object} options
   * An object describing the process to launch.
   *
   * @param {string} options.command
   * The full path of the execuable to launch. Relative paths are not
   * accepted, and `$PATH` is not searched.
   *
   * If a path search is necessary, the {@link Subprocess.pathSearch} method may
   * be used to map a bare executable name to a full path.
   *
   * @param {string[]} [options.arguments]
   * A list of strings to pass as arguments to the process.
   *
   * @param {object} [options.environment]
   * An object containing a key and value for each environment variable
   * to pass to the process. Only the object's own, enumerable properties
   * are added to the environment.
   *
   * @param {boolean} [options.environmentAppend]
   * If true, append the environment variables passed in `environment` to
   * the existing set of environment variables. Otherwise, the values in
   * 'environment' constitute the entire set of environment variables
   * passed to the new process.
   *
   * @param {string} [options.stderr]
   * Defines how the process's stderr output is handled. One of:
   *
   * - `"ignore"`: (default) The process's standard error is not redirected.
   * - `"stdout"`: The process's stderr is merged with its stdout.
   * - `"pipe"`: The process's stderr is redirected to a pipe, which can be read
   *   from via its `stderr` property.
   *
   * @param {string} [options.workdir]
   *        The working directory in which to launch the new process.
   *
   * @returns {Promise<Process>}
   *
   * @rejects {Error}
   * May be rejected with an Error object if the process can not be
   * launched. The object will include an `errorCode` property with
   * one of the following values if it was rejected for the
   * corresponding reason:
   *
   * - Subprocess.ERROR_BAD_EXECUTABLE: The given command could not
   *   be found, or the file that it references is not executable.
   *
   * Note that if the process is successfully launched, but exits with
   * a non-zero exit code, the promise will still resolve successfully.
   */
  call(options) {
    options = Object.assign({}, options);

    options.stderr = options.stderr || "ignore";
    options.workdir = options.workdir || null;

    let environment = {};
    if (!options.environment || options.environmentAppend) {
      environment = this.getEnvironment();
    }

    if (options.environment) {
      Object.assign(environment, options.environment);
    }

    options.environment = Object.entries(environment)
                                .map(([key, val]) => encodeEnvVar(key, val));

    options.arguments = Array.from(options.arguments || []);

    return Promise.resolve(SubprocessImpl.isExecutableFile(options.command)).then(isExecutable => {
      if (!isExecutable) {
        let error = new Error(`File at path "${options.command}" does not exist, or is not executable`);
        error.errorCode = SubprocessConstants.ERROR_BAD_EXECUTABLE;
        throw error;
      }

      options.arguments.unshift(options.command);

      return SubprocessImpl.call(options);
    });
  },

  /**
   * Returns an object with a key-value pair for every variable in the process's
   * current environment.
   *
   * @returns {object}
   */
  getEnvironment() {
    let environment = Object.create(null);
    for (let [k, v] of SubprocessImpl.getEnvironment()) {
      environment[k] = v;
    }
    return environment;
  },

  /**
   * Searches for the given executable file in the system executable
   * file paths as specified by the PATH environment variable.
   *
   * On Windows, if the unadorned filename cannot be found, the
   * extensions in the semicolon-separated list in the PATHSEP
   * environment variable are successively appended to the original
   * name and searched for in turn.
   *
   * @param {string} command
   *        The name of the executable to find.
   * @param {object} [environment]
   *        An object containing a key for each environment variable to be used
   *        in the search. If not provided, full the current process environment
   *        is used.
   * @returns {Promise<string>}
   */
  pathSearch(command, environment = this.getEnvironment()) {
    // Promise.resolve lets us get around returning one of the Promise.jsm
    // pseudo-promises returned by Task.jsm.
    let path = SubprocessImpl.pathSearch(command, environment);
    return Promise.resolve(path);
  },
};

Object.assign(Subprocess, SubprocessConstants);
Object.freeze(Subprocess);
