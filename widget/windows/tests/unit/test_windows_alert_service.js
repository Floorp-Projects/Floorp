/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that Windows alert notifications generate expected XML.
 */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
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
  return alert;
}

function testAlert(when, { serverEnabled, profD, isBackgroundTaskMode } = {}) {
  let argumentString = (argument, launchUrl, privilegedName) => {
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
    if (serverEnabled && launchUrl) {
      s += `&#xA;launchUrl&#xA;${launchUrl}`;
    }
    if (serverEnabled && privilegedName) {
      s += `&#xA;privilegedName&#xA;${privilegedName}`;
    }
    if (serverEnabled) {
      s += "&#xA;windowsTag&#xA;";
    }
    if (argument) {
      s += `&#xA;action&#xA;${argument}`;
    }
    return s;
  };

  let settingsAction = isBackgroundTaskMode
    ? ""
    : `<action content="Notification settings" arguments="${argumentString(
        "settings"
      )}" placement="contextmenu"/>`;

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

  let alert = makeAlert({ name, title, text });
  let expected = `<toast launch="${argumentString()}"><visual><binding template="ToastGeneric"><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.equal(
    expected.replace("<actions></actions>", "<actions/>"),
    alertsService.getXmlStringForWindowsAlert(alert),
    when
  );

  alert = makeAlert({ name, title, text, imageURL });
  expected = `<toast launch="${argumentString()}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.equal(
    expected.replace("<actions></actions>", "<actions/>"),
    alertsService.getXmlStringForWindowsAlert(alert),
    when
  );

  alert = makeAlert({ name, title, text, imageURL, requireInteraction: true });
  expected = `<toast scenario="reminder" launch="${argumentString()}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}</actions></toast>`;
  Assert.equal(
    expected.replace("<actions></actions>", "<actions/>"),
    alertsService.getXmlStringForWindowsAlert(alert),
    when
  );

  alert = makeAlert({ name, title, text, imageURL, actions });
  expected = `<toast launch="${argumentString()}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsAction}<action content="title1" arguments="${argumentString(
    "action1"
  )}"/><action content="title2" arguments="${argumentString(
    "action2"
  )}"/></actions></toast>`;
  Assert.equal(
    expected.replace("<actions></actions>", "<actions/>"),
    alertsService.getXmlStringForWindowsAlert(alert),
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
  let settingsActionWithPrivilegedName = isBackgroundTaskMode
    ? ""
    : `<action content="Notification settings" arguments="${argumentString(
        "settings",
        null,
        name
      )}" placement="contextmenu"/>`;
  expected = `<toast launch="${argumentString(
    null,
    null,
    name
  )}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsActionWithPrivilegedName}<action content="dismissTitle" arguments="dismiss" activationType="system"/><action content="snoozeTitle" arguments="snooze" activationType="system"/></actions></toast>`;
  Assert.equal(
    expected,
    alertsService.getXmlStringForWindowsAlert(alert),
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
  expected = `<toast launch="${argumentString()}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text><text id="3" placement="attribution">via example.com</text></binding></visual><actions><action content="Disable notifications from example.com" arguments="${argumentString(
    "snooze"
  )}" placement="contextmenu"/>${settingsAction}<action content="dismissTitle" arguments="${argumentString(
    "dismiss"
  )}"/><action content="snoozeTitle" arguments="${argumentString(
    "snooze"
  )}"/></actions></toast>`;
  Assert.equal(
    expected,
    alertsService.getXmlStringForWindowsAlert(alert),
    when
  );

  // Chrome privileged alerts can set a launch URL.
  alert = makeAlert({
    name,
    title,
    text,
    imageURL,
    principal: systemPrincipal,
  });
  alert.launchURL = launchUrl;
  let settingsActionWithLaunchUrl = isBackgroundTaskMode
    ? ""
    : `<action content="Notification settings" arguments="${argumentString(
        "settings",
        launchUrl,
        name
      )}" placement="contextmenu"/>`;
  expected = `<toast launch="${argumentString(
    null,
    launchUrl,
    name
  )}"><visual><binding template="ToastGeneric"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions>${settingsActionWithLaunchUrl}</actions></toast>`;
  Assert.equal(
    expected.replace("<actions></actions>", "<actions/>"),
    alertsService.getXmlStringForWindowsAlert(alert),
    when
  );
}

add_task(async () => {
  Services.prefs.clearUserPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled"
  );
  testAlert("when notification server pref is unset (i.e., default)", {
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
