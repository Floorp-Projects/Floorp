/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that Windows alert notifications generate expected XML.
 */

var { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

let gProfD = do_get_profile();

// Setup that allows to use the profile service in xpcshell tests,
// lifted from `toolkit/profile/xpcshell/head.js`.
function setupProfileService() {
  let gDataHome = gProfD.clone();
  gDataHome.append("data");
  gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let gDataHomeLocal = gProfD.clone();
  gDataHomeLocal.append("local");
  gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

  let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
    Ci.nsIXREDirProvider
  );
  xreDirProvider.setUserDataDirectory(gDataHome, false);
  xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);
}

add_setup(setupProfileService);

function makeAlert(options) {
  var alert = Cc["@mozilla.org/alert-notification;1"].createInstance(
    Ci.nsIAlertNotification
  );
  alert.init(
    options.name,
    options.imageURL,
    options.title,
    options.text,
    options.textClickable,
    options.cookie,
    options.dir,
    options.lang,
    options.data,
    options.principal,
    options.inPrivateBrowsing,
    options.requireInteraction,
    options.silent,
    options.vibrate || []
  );
  if (options.actions) {
    alert.actions = options.actions;
  }
  if (options.opaqueRelaunchData) {
    alert.opaqueRelaunchData = options.opaqueRelaunchData;
  }
  return alert;
}

/**
 * Take a `key1\nvalue1\n...` string encoding as used by the Windows native
 * notification server DLL, and split it into an object, keeping `action\n...`
 * intact.
 *
 * @param {string} t string encoding.
 * @returns {object} an object with keys and values.
 */
function parseOneEncoded(t) {
  var launch = {};

  var lines = t.split("\n");
  while (lines.length) {
    var key = lines.shift();
    var value;
    if (key === "action") {
      value = lines.join("\n");
      lines = [];
    } else {
      value = lines.shift();
    }
    launch[key] = value;
  }

  return launch;
}

/**
 * This complicated-looking function takes a (XML) string representation of a
 * Windows alert (toast notification), parses it into XML, extracts and further
 * parses internal data, and returns a simplified XML representation together
 * with the parsed internals.
 *
 * Doing this lets us compare JSON objects rather than stringified-JSON further
 * encoded as XML strings, which have lots of slashes and `&quot;` characters to
 * contend with.
 *
 * @param {string} s XML string for Windows alert.

 * @returns {Array} a pair of a simplified XML string and an object with
 *                  `launch` and `actions` keys.
 */
function parseLaunchAndActions(s) {
  var document = new DOMParser().parseFromString(s, "text/xml");
  var root = document.documentElement;

  var launchString = root.getAttribute("launch");
  root.setAttribute("launch", "launch");
  var launch = parseOneEncoded(launchString);

  // `actions` is keyed by "content" attribute.
  let actions = {};
  for (var actionElement of root.querySelectorAll("action")) {
    // `activationType="system"` is special.  Leave them alone.
    let systemActivationType =
      actionElement.getAttribute("activationType") === "system";

    let action = {};
    let names = [...actionElement.attributes].map(attribute => attribute.name);

    for (var name of names) {
      let value = actionElement.getAttribute(name);

      // Here is where we parse stringified-JSON to simplify comparisons.
      if (value.startsWith("{")) {
        value = JSON.parse(value);
        if ("opaqueRelaunchData" in value) {
          value.opaqueRelaunchData = JSON.parse(value.opaqueRelaunchData);
        }
      }

      if (name == "arguments" && !systemActivationType) {
        action[name] = parseOneEncoded(value);
      } else {
        action[name] = value;
      }

      if (name != "content" && !systemActivationType) {
        actionElement.removeAttribute(name);
      }
    }

    let actionName = actionElement.getAttribute("content");
    actions[actionName] = action;
  }

  return [new XMLSerializer().serializeToString(document), { launch, actions }];
}

function escape(s) {
  return s
    .replace(/&/g, "&amp;")
    .replace(/"/g, "&quot;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\n/g, "&#xA;");
}

function unescape(s) {
  return s
    .replace(/&amp;/g, "&")
    .replace(/&quot;/g, '"')
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&#xA;/g, "\n");
}

function testAlert(when, { serverEnabled, profD, isBackgroundTaskMode } = {}) {
  let argumentString = action => {
    // &#xA; is "\n".
    let s = ``;
    if (serverEnabled) {
      s += `program&#xA;${AppConstants.MOZ_APP_NAME}`;
    } else {
      s += `invalid key&#xA;invalid value`;
    }
    if (serverEnabled && profD) {
      s += `&#xA;profile&#xA;${profD.path}`;
    }
    if (serverEnabled) {
      s += "&#xA;windowsTag&#xA;";
    }
    if (action) {
      s += `&#xA;action&#xA;${escape(JSON.stringify(action))}`;
    }

    return s;
  };

  let parsedArgumentString = action =>
    parseOneEncoded(unescape(argumentString(action)));

  let settingsAction = isBackgroundTaskMode
    ? ""
    : `<action content="Notification settings"/>`;

  let parsedSettingsAction = hostport => {
    if (isBackgroundTaskMode) {
      return [];
    }
    let content = "Notification settings";
    return [
      content,
      {
        content,
        arguments: parsedArgumentString(
          Object.assign(
            {
              action: "settings",
            },
            hostport && {
              launchUrl: hostport,
            }
          )
        ),
        placement: "contextmenu",
      },
    ];
  };

  let parsedSnoozeAction = hostport => {
    let content = `Disable notifications from ${hostport}`;
    return [
      content,
      {
        content,
        arguments: parsedArgumentString(
          Object.assign(
            {
              action: "snooze",
            },
            hostport && {
              launchUrl: hostport,
            }
          )
        ),
        placement: "contextmenu",
      },
    ];
  };

  let alertsService = Cc["@mozilla.org/system-alerts-service;1"]
    .getService(Ci.nsIAlertsService)
    .QueryInterface(Ci.nsIWindowsAlertsService);

  let name = "name";
  let title = "title";
  let text = "text";
  let imageURL = "file:///image.png";
  let actions = [
    { action: "action1", title: "title1", iconURL: "file:///iconURL1.png" },
    { action: "action2", title: "title2", iconURL: "file:///iconURL2.png" },
  ];
  let opaqueRelaunchData = { foo: 1, bar: "two" };

  let alert = makeAlert({ name, title, text });
  let expected = `<toast launch="launch"><visual><binding template="ToastText03"><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "" }),
        actions: Object.fromEntries(
          [parsedSettingsAction()].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  alert = makeAlert({ name, title, text, imageURL });
  expected = `<toast launch="launch"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "" }),
        actions: Object.fromEntries(
          [parsedSettingsAction()].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  alert = makeAlert({ name, title, text, imageURL, requireInteraction: true });
  expected = `<toast scenario="reminder" launch="launch"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}<action content="Dismiss"/></actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "" }),
        actions: Object.fromEntries(
          [
            parsedSettingsAction(),
            [
              "Dismiss",
              {
                content: "Dismiss",
                arguments: { "": undefined },
                activationType: "background",
              },
            ],
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  alert = makeAlert({ name, title, text, imageURL, actions });
  expected = `<toast launch="launch"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}<action content="title1"/><action content="title2"/></actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "" }),
        actions: Object.fromEntries(
          [
            parsedSettingsAction(),
            [
              "title1",
              {
                content: "title1",
                arguments: parsedArgumentString({ action: "action1" }),
              },
            ],
            [
              "title2",
              {
                content: "title2",
                arguments: parsedArgumentString({ action: "action2" }),
              },
            ],
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  // Chrome privileged alerts can use `windowsSystemActivationType`.
  let systemActions = [
    {
      action: "dismiss",
      title: "dismissTitle",
      windowsSystemActivationType: true,
    },
    {
      action: "snooze",
      title: "snoozeTitle",
      windowsSystemActivationType: true,
    },
  ];
  let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    principal: systemPrincipal,
    actions: systemActions,
  });
  let parsedSettingsActionWithPrivilegedName = isBackgroundTaskMode
    ? []
    : [
        "Notification settings",
        {
          content: "Notification settings",
          arguments: parsedArgumentString({
            action: "settings",
            privilegedName: name,
          }),
          placement: "contextmenu",
        },
      ];

  expected = `<toast launch="launch"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}<action content="dismissTitle" arguments="dismiss" activationType="system"/><action content="snoozeTitle" arguments="snooze" activationType="system"/></actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "", privilegedName: name }),
        actions: Object.fromEntries(
          [
            parsedSettingsActionWithPrivilegedName,
            [
              "dismissTitle",
              {
                content: "dismissTitle",
                arguments: "dismiss",
                activationType: "system",
              },
            ],
            [
              "snoozeTitle",
              {
                content: "snoozeTitle",
                arguments: "snooze",
                activationType: "system",
              },
            ],
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  // But content unprivileged alerts can't use `windowsSystemActivationType`.
  let launchUrl = "https://example.com/foo/bar.html";
  const principaluri = Services.io.newURI(launchUrl);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    principaluri,
    {}
  );

  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    actions: systemActions,
    principal,
  });
  expected = `<toast launch="launch"><visual><binding template="ToastImageAndText04"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text><text id="3" placement="attribution">via example.com</text></binding></visual><actions><action content="Disable notifications from example.com"/>${settingsAction}<action content="dismissTitle"/><action content="snoozeTitle"/></actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({
          action: "",
          launchUrl: principaluri.hostPort,
        }),
        actions: Object.fromEntries(
          [
            parsedSnoozeAction(principaluri.hostPort),
            parsedSettingsAction(principaluri.hostPort),
            [
              "dismissTitle",
              {
                content: "dismissTitle",
                arguments: parsedArgumentString({
                  action: "dismiss",
                  launchUrl: principaluri.hostPort,
                }),
              },
            ],
            [
              "snoozeTitle",
              {
                content: "snoozeTitle",
                arguments: parsedArgumentString({
                  action: "snooze",
                  launchUrl: principaluri.hostPort,
                }),
              },
            ],
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  // Chrome privileged alerts can set `opaqueRelaunchData`.
  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    principal: systemPrincipal,
    opaqueRelaunchData: JSON.stringify(opaqueRelaunchData),
  });
  expected = `<toast launch="launch"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({
          action: "",
          opaqueRelaunchData: JSON.stringify(opaqueRelaunchData),
          privilegedName: name,
        }),
        actions: Object.fromEntries(
          [parsedSettingsActionWithPrivilegedName].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  // But content unprivileged alerts can't set `opaqueRelaunchData`.
  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    principal,
    opaqueRelaunchData: JSON.stringify(opaqueRelaunchData),
  });
  expected = `<toast launch="launch"><visual><binding template="ToastImageAndText04"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text><text id="3" placement="attribution">via example.com</text></binding></visual><actions><action content="Disable notifications from example.com"/>${settingsAction}</actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({
          action: "",
          launchUrl: principaluri.hostPort,
        }),
        actions: Object.fromEntries(
          [
            parsedSnoozeAction(principaluri.hostPort),
            parsedSettingsAction(principaluri.hostPort),
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );

  // Chrome privileged alerts can set action-specific relaunch parameters.
  let systemRelaunchActions = [
    {
      action: "action1",
      title: "title1",
      opaqueRelaunchData: JSON.stringify({ json: "data1" }),
    },
    {
      action: "action2",
      title: "title2",
      opaqueRelaunchData: JSON.stringify({ json: "data2" }),
    },
  ];
  systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    principal: systemPrincipal,
    actions: systemRelaunchActions,
  });
  expected = `<toast launch="launch"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}<action content="title1"/><action content="title2"/></actions></toast>`;
  Assert.deepEqual(
    [
      expected.replace("<actions></actions>", "<actions/>"),
      {
        launch: parsedArgumentString({ action: "", privilegedName: name }),
        actions: Object.fromEntries(
          [
            parsedSettingsActionWithPrivilegedName,
            [
              "title1",
              {
                content: "title1",
                arguments: parsedArgumentString(
                  {
                    action: "action1",
                    opaqueRelaunchData: JSON.stringify({ json: "data1" }),
                    privilegedName: name,
                  },
                  null,
                  name
                ),
              },
            ],

            [
              "title2",
              {
                content: "title2",
                arguments: parsedArgumentString(
                  {
                    action: "action2",
                    opaqueRelaunchData: JSON.stringify({ json: "data2" }),
                    privilegedName: name,
                  },
                  null,
                  name
                ),
              },
            ],
          ].filter(x => x.length)
        ),
      },
    ],
    parseLaunchAndActions(alertsService.getXmlStringForWindowsAlert(alert)),
    when
  );
}

add_task(async () => {
  Services.prefs.deleteBranch(
    "alerts.useSystemBackend.windows.notificationserver.enabled"
  );
  testAlert("when notification server pref is unset", {
    profD: gProfD,
  });

  Services.prefs.setBoolPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled",
    false
  );
  testAlert("when notification server pref is false", { profD: gProfD });

  Services.prefs.setBoolPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled",
    true
  );
  testAlert("when notification server pref is true", {
    serverEnabled: true,
    profD: gProfD,
  });
});

let condition = {
  skip_if: () => !AppConstants.MOZ_BACKGROUNDTASKS,
};

add_task(condition, async () => {
  const bts = Cc["@mozilla.org/backgroundtasks;1"]?.getService(
    Ci.nsIBackgroundTasks
  );

  // Pretend that this is a background task.
  bts.overrideBackgroundTaskNameForTesting("taskname");

  Services.prefs.setBoolPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled",
    true
  );
  testAlert(
    "when notification server pref is true in background task, no default profile",
    { serverEnabled: true, isBackgroundTaskMode: true }
  );

  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );

  let profilePath = do_get_profile();
  profilePath.append(`test_windows_alert_service`);
  let profile = profileService.createUniqueProfile(
    profilePath,
    "test_windows_alert_service"
  );

  profileService.defaultProfile = profile;

  testAlert(
    "when notification server pref is true in background task, default profile",
    { serverEnabled: true, isBackgroundTaskMode: true, profD: profilePath }
  );

  // No longer a background task,
  bts.overrideBackgroundTaskNameForTesting("");
});
