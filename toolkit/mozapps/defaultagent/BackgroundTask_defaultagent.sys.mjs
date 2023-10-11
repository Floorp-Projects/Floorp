/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EXIT_CODE as EXIT_CODE_BASE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";
import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

const EXIT_CODE = {
  ...EXIT_CODE_BASE,
  DISABLED_BY_POLICY: EXIT_CODE_BASE.LAST_RESERVED + 1,
  INVALID_ARGUMENT: EXIT_CODE_BASE.LAST_RESERVED + 2,
};

// Should be slightly longer than NOTIFICATION_WAIT_TIMEOUT_MS in
// Notification.cpp (divided by 1000 to convert millseconds to seconds) to not
// cause race between timeouts. Currently 12 hours + 10 additional seconds.
export const backgroundTaskTimeoutSec = 12 * 60 * 60 + 10;

// We expect to be given a command string in argv[1], perhaps followed by other
// arguments depending on the command. The valid commands are:
// register-task [unique-token]
//   Create a Windows scheduled task that will launch this binary with the
//   do-task command every 24 hours, starting from 24 hours after register-task
//   is run. unique-token is required and should be some string that uniquely
//   identifies this installation of the product; typically this will be the
//   install path hash that's used for the update directory, the AppUserModelID,
//   and other related purposes.
// update-task [unique-token]
//   Update an existing task registration, without changing its schedule. This
//   should be called during updates of the application, in case this program
//   has been updated and any of the task parameters have changed. The unique
//   token argument is required and should be the same one that was passed in
//   when the task was registered.
// unregister-task [unique-token]
//   Removes the previously created task. The unique token argument is required
//   and should be the same one that was passed in when the task was registered.
// uninstall [unique-token]
//   Removes the previously created task, and also removes all registry entries
//   running the task may have created. The unique token argument is required
//   and should be the same one that was passed in when the task was registered.
// do-task [app-user-model-id]
//   Actually performs the default agent task, which currently means generating
//   and sending our telemetry ping and possibly showing a notification to the
//   user if their browser has switched from Firefox to Edge with Blink.
// set-default-browser-user-choice [app-user-model-id] [[.file1 ProgIDRoot1]
// ...]
//   Set the default browser via the UserChoice registry keys.  Additional
//   optional file extensions to register can be specified as additional
//   argument pairs: the first element is the file extension, the second element
//   is the root of a ProgID, which will be suffixed with `-$AUMI`.
export async function runBackgroundTask(commandLine) {
  Services.fog.initializeFOG(
    undefined,
    "firefox.desktop.background.defaultagent"
  );

  let defaultAgent = Cc["@mozilla.org/default-agent;1"].getService(
    Ci.nsIDefaultAgent
  );

  let command = commandLine.getArgument(0);

  // The uninstall and unregister commands are allowed even if the policy
  // disabling the task is set, so that uninstalls and updates always work.
  // Similarly, debug commands are always allowed.
  switch (command) {
    case "uninstall": {
      let token = commandLine.getArgument(1);
      defaultAgent.uninstall(token);
      return EXIT_CODE.SUCCESS;
    }
    case "unregister-task": {
      let token = commandLine.getArgument(1);
      defaultAgent.unregisterTask(token);
      return EXIT_CODE.SUCCESS;
    }
  }

  // We check for disablement by policy because that's assumed to be static.
  // But we don't check for disablement by remote settings so that
  // `register-task` and `update-task` can proceed as part of the update
  // cycle, waiting for remote (re-)enablement.
  if (defaultAgent.agentDisabled()) {
    return EXIT_CODE.DISABLED_BY_POLICY;
  }

  switch (command) {
    case "register-task": {
      let token = commandLine.getArgument(1);
      defaultAgent.registerTask(token);
      return EXIT_CODE.SUCCESS;
    }
    case "update-task": {
      let token = commandLine.getArgument(1);
      defaultAgent.updateTask(token);
      return EXIT_CODE.SUCCESS;
    }
    case "do-task": {
      let aumid = commandLine.getArgument(1);
      let force = commandLine.findFlag("force", true) != -1;
      defaultAgent.doTask(aumid, force);

      // Bug 1857333: We wait for arbitrary time for Glean to submit telemetry.
      console.error("Pinged glean, waiting for submission.");
      await new Promise(resolve => setTimeout(resolve, 5000));

      return EXIT_CODE.SUCCESS;
    }
  }

  return EXIT_CODE.INVALID_ARGUMENT;
}
