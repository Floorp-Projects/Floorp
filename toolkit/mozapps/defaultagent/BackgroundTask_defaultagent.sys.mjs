/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EXIT_CODE as EXIT_CODE_BASE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";
import { AppConstants as AC } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const EXIT_CODE = {
  ...EXIT_CODE_BASE,
  DISABLED_BY_POLICY: EXIT_CODE_BASE.LAST_RESERVED + 1,
  INVALID_ARGUMENT: EXIT_CODE_BASE.LAST_RESERVED + 2,
  MUTEX_NOT_LOCKABLE: EXIT_CODE_BASE.LAST_RESERVED + 3,
};

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  BackgroundTasksUtils: "resource://gre/modules/BackgroundTasksUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});
XPCOMUtils.defineLazyServiceGetters(lazy, {
  AlertsService: ["@mozilla.org/alerts-service;1", "nsIAlertsService"],
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
    maxLogLevel: "error",
    maxLogLevelPref: "app.defaultagent.loglevel",
    prefix: "DefaultAgent",
  };
  return new ConsoleAPI(consoleOptions);
});

// Should be slightly longer than NOTIFICATION_WAIT_TIMEOUT_MS in
// Notification.cpp (divided by 1000 to convert millseconds to seconds) to not
// cause race between timeouts. Currently 12 hours + 10 additional seconds.
export const backgroundTaskTimeoutSec = 12 * 60 * 60 + 10;
const kNotificationTimeoutMs = 12 * 60 * 60 * 1000;

const kNotificationShown = Object.freeze({
  notShown: "not-shown",
  shown: "shown",
  error: "error",
});

const kNotificationAction = Object.freeze({
  dismissedByTimeout: "dismissed-by-timeout",
  dismissedByButton: "dismissed-by-button",
  dismissedToActionCenter: "dismissed-to-action-center",
  makeFirefoxDefaultButton: "make-firefox-default-button",
  toastClicked: "toast-clicked",
  noAction: "no-action",
});

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
      lazy.log.info(`Uninstalling for token "${token}"`);
      let token = commandLine.getArgument(1);
      defaultAgent.uninstall(token);
      return EXIT_CODE.SUCCESS;
    }
    case "unregister-task": {
      lazy.log.info(`Unregistering task for token "${token}"`);
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
    lazy.log.warn("Default Agent disabled, exiting without running.");
    return EXIT_CODE.DISABLED_BY_POLICY;
  }

  switch (command) {
    case "register-task": {
      lazy.log.info(`Registering task for token "${token}"`);
      let token = commandLine.getArgument(1);
      defaultAgent.registerTask(token);
      return EXIT_CODE.SUCCESS;
    }
    case "update-task": {
      lazy.log.info(`Updating task for token "${token}"`);
      let token = commandLine.getArgument(1);
      defaultAgent.updateTask(token);
      return EXIT_CODE.SUCCESS;
    }
    case "do-task": {
      let aumid = commandLine.getArgument(1);
      let force = commandLine.findFlag("force", true) != -1;

      lazy.log.info(`Running do-task with AUMID "${aumid}"`);

      let cppFallback = false;
      try {
        await lazy.BackgroundTasksUtils.enableNimbus(commandLine);
        cppFallback =
          lazy.NimbusFeatures.defaultAgent.getVariable("cppFallback");
      } catch (e) {
        lazy.log.error(`Error enabling nimbus: ${e}`);
      }

      try {
        if (!cppFallback) {
          lazy.log.info("Running JS do-task.");
          await runWithRegistryLocked(async () => {
            await doTask(defaultAgent, force);
          });
        } else {
          lazy.log.info("Running C++ do-task.");
          defaultAgent.doTask(aumid, force);
        }
      } catch (e) {
        if (e.message) {
          lazy.log.error(e.message);
        }

        if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
          return EXIT_CODE.MUTEX_NOT_LOCKABLE;
        }

        return EXIT_CODE.EXCEPTION;
      }

      // Bug 1857333: We wait for arbitrary time for Glean to submit telemetry.
      lazy.log.info("Pinged glean, waiting for submission.");
      await new Promise(resolve => lazy.setTimeout(resolve, 5000));

      return EXIT_CODE.SUCCESS;
    }
  }

  return EXIT_CODE.INVALID_ARGUMENT;
}

// Throws if unable to lock mutex (therefore function isn't run).
async function runWithRegistryLocked(aMutexGuardedFunction) {
  const kVendor = Services.appinfo.vendor || "";
  const kRegistryMutexName = `${kVendor}${AC.MOZ_APP_BASENAME}DefaultBrowserAgentRegistryMutex`;
  let mutexFactory = Cc["@mozilla.org/windows-mutex-factory;1"].getService(
    Ci.nsIWindowsMutexFactory
  );

  let mutex = mutexFactory.createMutex(kRegistryMutexName);
  mutex.tryLock(kRegistryMutexName);
  lazy.log.debug(`Locked named mutex: ${kRegistryMutexName}`);
  try {
    await aMutexGuardedFunction();
  } finally {
    mutex.unlock();
    lazy.log.debug(`Unlocked named mutex: ${kRegistryMutexName}`);
  }
}

async function doTask(defaultAgent, force) {
  if (!defaultAgent.appRanRecently() && !force) {
    lazy.log.warn("Main app has not ran recently, exiting without running.");
    throw new Error("App hasn't ran recently");
  }

  let browser = defaultAgent.getDefaultBrowser();
  lazy.log.debug(`Default browser: ${browser}`);
  let previousBrowser = defaultAgent.getReplacePreviousDefaultBrowser(browser);
  lazy.log.debug(`Previous browser: ${previousBrowser}`);
  let defaultPdfHandler = defaultAgent.getDefaultPdfHandler();
  lazy.log.debug(`Default PDF Handler: ${defaultPdfHandler}`);

  let notificationTelemetry = {
    shown: kNotificationShown.notShown,
    action: kNotificationAction.noAction,
  };
  if ((browser == "edge-chrome" && previousBrowser == "firefox") || force) {
    lazy.log.info("Showing default browser intervention notification.");

    const alertName = "default_agent_intervention";
    let notification = showNotification(alertName);
    let timeout = makeTimeout(alertName);

    notificationTelemetry = await Promise.race([notification, timeout]);
  }
  lazy.log.debug(`Notification telemetry: ${notificationTelemetry}`);

  if (
    notificationTelemetry.action ==
      kNotificationAction.makeFirefoxDefaultButton ||
    notificationTelemetry.action == kNotificationAction.toastClicked
  ) {
    let aumid = lazy.XreDirProvider.getInstallHash();
    lazy.log.info(`Setting default browser with AUMID: ${aumid}`);
    defaultAgent.setDefaultBrowserUserChoice(aumid, []);
  }

  defaultAgent.sendPing(
    browser,
    previousBrowser,
    defaultPdfHandler,
    notificationTelemetry.shown,
    notificationTelemetry.action
  );
}

async function showNotification(name) {
  let notificationTelemetry = {
    shown: kNotificationShown.error,
    action: kNotificationAction.noAction,
  };

  // Bug 1868714: We disable the notification server to defer on changes
  // necessary for it to work with Background Tasks.
  try {
    lazy.log.debug("Disabling notification server.");
    Services.prefs.setBoolPref(
      "alerts.useSystemBackend.windows.notificationserver.enabled",
      false
    );

    const l10n = new Localization([
      "branding/brand.ftl",
      // Background tasks are only used in a context where browser refs are
      // present; that it's in toolkit instead of browser is a historical
      // artifact of the default agent having previously been a
      // standalone application.
      // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
      "browser/backgroundtasks/defaultagent.ftl",
    ]);
    let [title, body, yesButtonText, noButtonText] = await l10n.formatValues([
      { id: "default-browser-notification-header-text" },
      { id: "default-browser-notification-body-text" },
      { id: "default-browser-notification-yes-button-text" },
      { id: "default-browser-notification-no-button-text" },
    ]);

    let yesAction = "yes-action";
    let noAction = "no-action";

    let alert = makeAlert({
      name,
      title,
      body,
      actions: [
        {
          action: yesAction,
          title: yesButtonText,
        },
        {
          action: noAction,
          title: noButtonText,
        },
      ],
    });

    const { observer, shownPromise } = makeObserver({ yesAction, noAction });

    lazy.AlertsService.showAlert(alert, observer);

    notificationTelemetry = await shownPromise.promise;
  } catch (e) {
    if (e.message) {
      lazy.log.error(e.message);
    }
  } finally {
    // Reset the pref so we can assume the default value in the future.
    lazy.log.debug("Reenabling notification server.");
    Services.prefs.clearUserPref(
      "alerts.useSystemBackend.windows.notificationserver.enabled"
    );
  }

  return notificationTelemetry;
}

function makeAlert(options) {
  let winalert = Cc["@mozilla.org/windows-alert-notification;1"].createInstance(
    Ci.nsIWindowsAlertNotification
  );
  winalert.handleActions = true;
  winalert.imagePlacement = winalert.eIcon;

  let alert = winalert.QueryInterface(Ci.nsIAlertNotification);
  let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  alert.init(
    options.name,
    "chrome://branding/content/about-logo@2x.png",
    options.title,
    options.body,
    true /* aTextClickable */,
    null /* aCookie */,
    null /* aDir */,
    null /* aLang */,
    null /* aData */,
    systemPrincipal,
    null /* aInPrivateBrowsing */,
    true /* aRequireInteraction */
  );

  alert.actions = options.actions;

  return alert;
}

function makeObserver(actions) {
  let shownPromise = Promise.withResolvers();

  // We'll receive multiple callbacks which individually might indicate an
  // interaction. Only log the first one to disambiguate and reduce noise.
  let firstInteraction = true;
  let logFirstInteraction = message => {
    if (firstInteraction) {
      lazy.log.debug(message);
      firstInteraction = false;
    }
  };

  let observer = (subject, topic, data) => {
    switch (topic) {
      case "alertactioncallback":
        switch (data) {
          case actions.yesAction:
            logFirstInteraction(
              'Notification "yes" button clicked, setting default browser.'
            );
            shownPromise.resolve({
              shown: kNotificationShown.shown,
              action: kNotificationAction.makeFirefoxDefaultButton,
            });
            break;
          case actions.noAction:
            logFirstInteraction("Notification dismissed by button.");
            shownPromise.resolve({
              shown: kNotificationShown.shown,
              action: kNotificationAction.dismissedByButton,
            });
            break;
          default:
            lazy.log.error(`Unrecognized notification action ${data}`);
            throw new Error(`Unexpected notification action received: ${data}`);
        }
        break;
      case "alertclickcallback":
        logFirstInteraction(
          "Notification body clicked, setting default browser."
        );
        shownPromise.resolve({
          shown: kNotificationShown.shown,
          action: kNotificationAction.toastClicked,
        });
        break;
      case "alerterror":
        lazy.log.error("Error showing notification.");
        shownPromise.resolve({
          shown: kNotificationShown.error,
          action: kNotificationAction.noAction,
        });
        break;
      case "alertfinished":
        logFirstInteraction("Notification dismissed from action center.");
        shownPromise.resolve({
          shown: kNotificationShown.shown,
          action: kNotificationAction.dismissedToActionCenter,
        });
        break;
    }
  };

  return { observer, shownPromise };
}

function makeTimeout(alertName) {
  return new Promise(resolve => {
    // If the notification hasn't been activated or dismissed within 12 hours,
    // stop waiting for it.
    let timeoutMs = kNotificationTimeoutMs;

    // Allow overriding the notification timeout fron an environment variable.
    const envTimeoutKey = "MOZ_NOTIFICATION_TIMEOUT_MS";
    if (Services.env.exists(envTimeoutKey)) {
      let envTimeoutValue = Services.env.get(envTimeoutKey);
      if (!isNaN(envTimeoutValue)) {
        timeoutMs = Number(envTimeoutValue);
      } else {
        lazy.log.error(
          `Environment variable ${envTimeoutKey}=${envTimeoutValue} is not a number.`
        );
      }
    }
    lazy.log.info(`Registering notification timeout in ${timeoutMs}ms`);

    lazy.setTimeout(() => {
      lazy.log.warn(`Notification timed out after ${timeoutMs}ms`);

      lazy.AlertsService.closeAlert(alertName);

      resolve({
        shown: kNotificationShown.shown,
        action: kNotificationAction.dismissedByTimeout,
      });
    }, timeoutMs);
  });
}
