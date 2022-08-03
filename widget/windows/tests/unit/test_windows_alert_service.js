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
  alert.initActions(options.actions || []);
  return alert;
}

function testAlert(serverEnabled) {
  let argumentString = argument => {
    // &#xA; is "\n".
    let s = ``;
    if (serverEnabled) {
      s += `program&#xA;${AppConstants.MOZ_APP_NAME}&#xA;profile&#xA;${gProfD.path}`;
    } else {
      s += `invalid key&#xA;invalid value`;
    }
    if (argument) {
      s += `&#xA;action&#xA;${argument}`;
    }
    return s;
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

  let alert = makeAlert({ name, title, text });
  let expected = `<toast launch="${argumentString()}"><visual><binding template="ToastText03"><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="${argumentString(
    "settings"
  )}" placement="contextmenu"/></actions></toast>`;
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL });
  expected = `<toast launch="${argumentString()}"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="${argumentString(
    "settings"
  )}" placement="contextmenu"/></actions></toast>`;
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL, requireInteraction: true });
  expected = `<toast scenario="reminder" launch="${argumentString()}"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="${argumentString(
    "settings"
  )}" placement="contextmenu"/></actions></toast>`;
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL, actions });
  expected = `<toast launch="${argumentString()}"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="${argumentString(
    "settings"
  )}" placement="contextmenu"/><action content="title1" arguments="${argumentString(
    "action1"
  )}"/><action content="title2" arguments="${argumentString(
    "action2"
  )}"/></actions></toast>`;
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));
}

add_task(async () => {
  Services.prefs.clearUserPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled"
  );
  testAlert(false);

  Services.prefs.setBoolPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled",
    false
  );
  testAlert(false);

  Services.prefs.setBoolPref(
    "alerts.useSystemBackend.windows.notificationserver.enabled",
    true
  );
  testAlert(true);
});
