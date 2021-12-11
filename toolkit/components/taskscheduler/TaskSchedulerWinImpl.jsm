/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["_TaskSchedulerWinImpl"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  WinTaskSvc: [
    "@mozilla.org/win-task-scheduler-service;1",
    "nsIWinTaskSchedulerService",
  ],
  XreDirProvider: [
    "@mozilla.org/xre/directory-provider;1",
    "nsIXREDirProvider",
  ],
});

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLSerializer"]);

/**
 * Task generation and management for Windows, using Task Scheduler 2.0 (taskschd).
 *
 * Implements the API exposed in TaskScheduler.jsm
 * Not intended for external use, this is in a separate module to ship the code only
 * on Windows, and to expose for testing.
 */
var _TaskSchedulerWinImpl = {
  registerTask(id, command, intervalSeconds, options) {
    // The folder might not yet exist.
    this._createFolderIfNonexistent();

    const xml = this._formatTaskDefinitionXML(
      command,
      intervalSeconds,
      options
    );
    const updateExisting = true;

    WinTaskSvc.registerTask(
      this._taskFolderName(),
      this._formatTaskName(id),
      xml,
      updateExisting
    );
  },

  deleteTask(id) {
    WinTaskSvc.deleteTask(this._taskFolderName(), this._formatTaskName(id));
  },

  /**
   * Delete all tasks created by this installation.
   *
   * The Windows Default Browser Agent task is special: it's
   * registered by the installer and might run as a different user and
   * require permissions to delete.  We ignore it and leave it for the
   * uninstaller to remove.
   */
  deleteAllTasks() {
    const taskFolderName = this._taskFolderName();

    let allTasks;
    try {
      allTasks = WinTaskSvc.getFolderTasks(taskFolderName);
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        // Folder doesn't exist, nothing to delete.
        return;
      }
      throw ex;
    }

    const tasksToDelete = allTasks.filter(name => this._matchAppTaskName(name));

    let numberDeleted = 0;
    let lastFailedTaskName;
    // We need `MOZ_APP_DISPLAYNAME` since that's what the WDBA (written in C++) uses.
    const defaultBrowserAgentTaskName =
      AppConstants.MOZ_APP_DISPLAYNAME_DO_NOT_USE +
      " Default Browser Agent " +
      XreDirProvider.getInstallHash();
    for (const taskName of tasksToDelete) {
      if (taskName == defaultBrowserAgentTaskName) {
        // Skip the Windows Default Browser Agent task.
        continue;
      }

      try {
        WinTaskSvc.deleteTask(taskFolderName, taskName);
        numberDeleted += 1;
      } catch (e) {
        lastFailedTaskName = taskName;
      }
    }

    if (lastFailedTaskName) {
      // There's no standard way to chain exceptions, so instead try again,
      // which should fail and throw again.  It's possible this isn't idempotent
      // but we're expecting failures to be due to permission errors, which are
      // likely to be static.
      WinTaskSvc.deleteTask(taskFolderName, lastFailedTaskName);
    }

    if (allTasks.length == numberDeleted) {
      // Deleted every task, remove the folder.
      this._deleteFolderIfEmpty();
    }
  },

  taskExists(id) {
    const taskFolderName = this._taskFolderName();

    let allTasks;
    try {
      allTasks = WinTaskSvc.getFolderTasks(taskFolderName);
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        // Folder doesn't exist, so neither do tasks within it.
        return false;
      }
      throw ex;
    }

    return allTasks.includes(this._formatTaskName(id));
  },

  _formatTaskDefinitionXML(command, intervalSeconds, options) {
    const startTime = new Date(Date.now() + intervalSeconds * 1000);
    const xmlns = "http://schemas.microsoft.com/windows/2004/02/mit/task";

    // Fill in the constant parts of the task, and those that don't require escaping.
    const docBase = `<Task xmlns="${xmlns}">
  <Triggers>
    <TimeTrigger>
      <StartBoundary>${startTime.toISOString()}</StartBoundary>
      <Repetition>
        <Interval>PT${intervalSeconds}S</Interval>
      </Repetition>
    </TimeTrigger>
  </Triggers>
  <Actions>
    <Exec />
  </Actions>
  <Settings>
    <StartWhenAvailable>true</StartWhenAvailable>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
  </Settings>
  <RegistrationInfo>
     <Author />
  </RegistrationInfo>
</Task>`;
    const doc = new DOMParser().parseFromString(docBase, "text/xml");

    const execAction = doc.querySelector("Actions Exec");

    const settings = doc.querySelector("Settings");

    const commandNode = doc.createElementNS(xmlns, "Command");
    commandNode.textContent = command;
    execAction.appendChild(commandNode);

    if (options?.args) {
      const args = doc.createElementNS(xmlns, "Arguments");
      args.textContent = options.args.map(this._quoteString).join(" ");
      execAction.appendChild(args);
    }

    if (options?.workingDirectory) {
      const workingDirectory = doc.createElementNS(xmlns, "WorkingDirectory");
      workingDirectory.textContent = options.workingDirectory;
      execAction.appendChild(workingDirectory);
    }

    if (options?.disabled) {
      const enabled = doc.createElementNS(xmlns, "Enabled");
      enabled.textContent = "false";
      settings.appendChild(enabled);
    }

    if (options?.executionTimeoutSec && options.executionTimeoutSec > 0) {
      const timeout = doc.createElementNS(xmlns, "ExecutionTimeLimit");
      timeout.textContent = `PT${options.executionTimeoutSec}S`;
      settings.appendChild(timeout);
    }

    // Other settings to consider for the future:
    // Idle
    // Battery

    doc.querySelector("RegistrationInfo Author").textContent =
      Services.appinfo.vendor;

    if (options?.description) {
      const registrationInfo = doc.querySelector("RegistrationInfo");
      const description = doc.createElementNS(xmlns, "Description");
      description.textContent = options.description;
      registrationInfo.appendChild(description);
    }

    const serializer = new XMLSerializer();
    return serializer.serializeToString(doc);
  },

  _createFolderIfNonexistent() {
    const { parentName, subName } = this._taskFolderNameParts();

    try {
      WinTaskSvc.createFolder(parentName, subName);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
        throw e;
      }
    }
  },

  _deleteFolderIfEmpty() {
    const { parentName, subName } = this._taskFolderNameParts();

    try {
      WinTaskSvc.deleteFolder(parentName, subName);
    } catch (e) {
      // Missed one somehow, possibly a subfolder?
      if (e.result != Cr.NS_ERROR_FILE_DIR_NOT_EMPTY) {
        throw e;
      }
    }
  },

  /**
   * Quotes a string for use as a single command argument, using Windows quoting
   * conventions.
   *
   * copied from quoteString() in toolkit/modules/subproces/subprocess_worker_win.js
   *
   *
   * @see https://msdn.microsoft.com/en-us/library/17w5ykft(v=vs.85).aspx
   *
   * @param {string} str
   *        The argument string to quote.
   * @returns {string}
   */
  _quoteString(str) {
    if (!/[\s"]/.test(str)) {
      return str;
    }

    let escaped = str.replace(/(\\*)("|$)/g, (m0, m1, m2) => {
      if (m2) {
        m2 = `\\${m2}`;
      }
      return `${m1}${m1}${m2}`;
    });

    return `"${escaped}"`;
  },

  _taskFolderName() {
    return `\\${Services.appinfo.vendor}`;
  },

  _taskFolderNameParts() {
    return {
      parentName: "\\",
      subName: Services.appinfo.vendor,
    };
  },

  _formatTaskName(id) {
    const installHash = XreDirProvider.getInstallHash();
    return `${id} ${installHash}`;
  },

  _matchAppTaskName(name) {
    const installHash = XreDirProvider.getInstallHash();
    return name.endsWith(` ${installHash}`);
  },
};
