/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Subprocess: "resource://gre/modules/Subprocess.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  XreDirProvider: [
    "@mozilla.org/xre/directory-provider;1",
    "nsIXREDirProvider",
  ],
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.sys.mjs for details.
    maxLogLevel: "error",
    maxLogLevelPref: "toolkit.components.taskscheduler.loglevel",
    prefix: "TaskScheduler",
  };
  return new ConsoleAPI(consoleOptions);
});

/**
 * Task generation and management for macOS, using `launchd` via `launchctl`.
 *
 * Implements the API exposed in TaskScheduler.jsm
 * Not intended for external use, this is in a separate module to ship the code only
 * on macOS, and to expose for testing.
 */
export var MacOSImpl = {
  async registerTask(id, command, intervalSeconds, options) {
    lazy.log.info(
      `registerTask(${id}, ${command}, ${intervalSeconds}, ${JSON.stringify(
        options
      )})`
    );

    let uid = await this._uid();
    lazy.log.debug(`registerTask: uid=${uid}`);

    let label = this._formatLabelForThisApp(id, options);

    // We ignore `options.disabled`, which is test only.
    //
    // The `Disabled` key prevents `launchd` from registering the task, with
    // exit code 133 and error message "Service is disabled".  If we really want
    // this flow in the future, there is `launchctl disable ...`, but it's
    // fraught with peril: the disabled status is stored outside of any plist,
    // and it persists even after the task is deleted.  Monkeying with the
    // disabled status will likely prevent users from disabling these tasks
    // forcibly, should it come to that.  All told, fraught.
    //
    // For the future: there is the `RunAtLoad` key, should we want to run the
    // task once immediately.
    let plist = {};
    plist.Label = label;
    plist.ProgramArguments = [command];
    if (options.args) {
      plist.ProgramArguments.push(...options.args);
    }
    plist.StartInterval = intervalSeconds;
    if (options.workingDirectory) {
      plist.WorkingDirectory = options.workingDirectory;
    }

    let str = this._formatLaunchdPlist(plist);
    let path = this._formatPlistPath(label);

    await IOUtils.write(path, new TextEncoder().encode(str));
    lazy.log.debug(`registerTask: wrote ${path}`);

    try {
      let bootout = await lazy.Subprocess.call({
        command: "/bin/launchctl",
        arguments: ["bootout", `gui/${uid}/${label}`],
        stderr: "stdout",
      });

      lazy.log.debug(
        "registerTask: bootout stdout",
        await bootout.stdout.readString()
      );

      let { exitCode } = await bootout.wait();
      lazy.log.debug(`registerTask: bootout returned ${exitCode}`);

      let bootstrap = await lazy.Subprocess.call({
        command: "/bin/launchctl",
        arguments: ["bootstrap", `gui/${uid}`, path],
        stderr: "stdout",
      });

      lazy.log.debug(
        "registerTask: bootstrap stdout",
        await bootstrap.stdout.readString()
      );

      ({ exitCode } = await bootstrap.wait());
      lazy.log.debug(`registerTask: bootstrap returned ${exitCode}`);

      if (exitCode != 0) {
        throw new Components.Exception(
          `Failed to run launchctl bootstrap: ${exitCode}`,
          Cr.NS_ERROR_UNEXPECTED
        );
      }
    } catch (e) {
      // Try to clean up.
      await IOUtils.remove(path, { ignoreAbsent: true });
      throw e;
    }

    return true;
  },

  async deleteTask(id, options) {
    lazy.log.info(`deleteTask(${id})`);

    let label = this._formatLabelForThisApp(id, options);
    return this._deleteTaskByLabel(label);
  },

  async _deleteTaskByLabel(label) {
    let path = this._formatPlistPath(label);
    lazy.log.debug(`_deleteTaskByLabel: removing ${path}`);
    await IOUtils.remove(path, { ignoreAbsent: true });

    let uid = await this._uid();
    lazy.log.debug(`_deleteTaskByLabel: uid=${uid}`);

    let bootout = await lazy.Subprocess.call({
      command: "/bin/launchctl",
      arguments: ["bootout", `gui/${uid}/${label}`],
      stderr: "stdout",
    });

    let { exitCode } = await bootout.wait();
    lazy.log.debug(`_deleteTaskByLabel: bootout returned ${exitCode}`);
    lazy.log.debug(
      `_deleteTaskByLabel: bootout stdout`,
      await bootout.stdout.readString()
    );

    return !exitCode;
  },

  // For internal and testing use only.
  async _listAllLabelsForThisApp() {
    let proc = await lazy.Subprocess.call({
      command: "/bin/launchctl",
      arguments: ["list"],
      stderr: "stdout",
    });

    let { exitCode } = await proc.wait();
    if (exitCode != 0) {
      throw new Components.Exception(
        `Failed to run /bin/launchctl list: ${exitCode}`,
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    let stdout = await proc.stdout.readString();

    let lines = stdout.split(/\r\n|\n|\r/);
    let labels = lines
      .map(line => line.split("\t").pop()) // Lines are like "-\t0\tlabel".
      .filter(this._labelMatchesThisApp);

    lazy.log.debug(`_listAllLabelsForThisApp`, labels);
    return labels;
  },

  async deleteAllTasks() {
    lazy.log.info(`deleteAllTasks()`);

    let labelsToDelete = await this._listAllLabelsForThisApp();

    let deleted = 0;
    let failed = 0;
    for (const label of labelsToDelete) {
      try {
        if (await this._deleteTaskByLabel(label)) {
          deleted += 1;
        } else {
          failed += 1;
        }
      } catch (e) {
        failed += 1;
      }
    }

    let result = { deleted, failed };
    lazy.log.debug(`deleteAllTasks: returning ${JSON.stringify(result)}`);
  },

  async taskExists(id, options) {
    const label = this._formatLabelForThisApp(id, options);
    const path = this._formatPlistPath(label);
    return IOUtils.exists(path);
  },

  /**
   * Turn an object into a macOS plist.
   *
   * Properties of type array-of-string, dict-of-string, string,
   * number, and boolean are supported.
   *
   * @param   options object to turn into macOS plist.
   * @returns plist as an XML DOM object.
   */
  _toLaunchdPlist(options) {
    const doc = new DOMParser().parseFromString("<plist></plist>", "text/xml");
    const root = doc.documentElement;
    root.setAttribute("version", "1.0");

    let dict = doc.createElement("dict");
    root.appendChild(dict);

    for (let [k, v] of Object.entries(options)) {
      let key = doc.createElement("key");
      key.textContent = k;
      dict.appendChild(key);

      if (Array.isArray(v)) {
        let array = doc.createElement("array");
        dict.appendChild(array);

        for (let vv of v) {
          let string = doc.createElement("string");
          string.textContent = vv;
          array.appendChild(string);
        }
      } else if (typeof v === "object") {
        let d = doc.createElement("dict");
        dict.appendChild(d);

        for (let [kk, vv] of Object.entries(v)) {
          key = doc.createElement("key");
          key.textContent = kk;
          d.appendChild(key);

          let string = doc.createElement("string");
          string.textContent = vv;
          d.appendChild(string);
        }
      } else if (typeof v === "number") {
        let number = doc.createElement(
          Number.isInteger(v) ? "integer" : "real"
        );
        number.textContent = v;
        dict.appendChild(number);
      } else if (typeof v === "string") {
        let string = doc.createElement("string");
        string.textContent = v;
        dict.appendChild(string);
      } else if (typeof v === "boolean") {
        let bool = doc.createElement(v ? "true" : "false");
        dict.appendChild(bool);
      }
    }

    return doc;
  },

  /**
   * Turn an object into a macOS plist encoded as a string.
   *
   * Properties of type array-of-string, dict-of-string, string,
   * number, and boolean are supported.
   *
   * @param   options object to turn into macOS plist.
   * @returns plist as a string.
   */
  _formatLaunchdPlist(options) {
    let doc = this._toLaunchdPlist(options);

    let serializer = new XMLSerializer();
    return serializer.serializeToString(doc);
  },

  _formatLabelForThisApp(id, options) {
    let installHash = lazy.XreDirProvider.getInstallHash();
    return `${AppConstants.MOZ_MACBUNDLE_ID}.${installHash}.${id}`;
  },

  _labelMatchesThisApp(label, options) {
    let installHash = lazy.XreDirProvider.getInstallHash();
    return (
      label &&
      label.startsWith(`${AppConstants.MOZ_MACBUNDLE_ID}.${installHash}.`)
    );
  },

  _formatPlistPath(label) {
    let file = Services.dirsvc.get("Home", Ci.nsIFile);
    file.append("Library");
    file.append("LaunchAgents");
    file.append(`${label}.plist`);
    return file.path;
  },

  _cachedUid: -1,

  async _uid() {
    if (this._cachedUid >= 0) {
      return this._cachedUid;
    }

    // There are standard APIs for determining our current UID, but this
    // is easy and parallel to the general tactics used by this module.
    let proc = await lazy.Subprocess.call({
      command: "/usr/bin/id",
      arguments: ["-u"],
      stderr: "stdout",
    });

    let stdout = await proc.stdout.readString();

    let { exitCode } = await proc.wait();
    if (exitCode != 0) {
      throw new Components.Exception(
        `Failed to run /usr/bin/id: ${exitCode}`,
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    try {
      this._cachedUid = Number.parseInt(stdout);
      return this._cachedUid;
    } catch (e) {
      throw new Components.Exception(
        `Failed to parse /usr/bin/id output as integer: ${stdout}`,
        Cr.NS_ERROR_UNEXPECTED
      );
    }
  },
};
