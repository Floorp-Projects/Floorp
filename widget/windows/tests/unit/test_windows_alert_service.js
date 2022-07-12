/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that Windows alert notifications generate expected XML.
 */

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

add_task(async () => {
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
  let expected =
    '<toast><visual><binding template="ToastText03"><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="settings" placement="contextmenu"/></actions></toast>';
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL });
  expected =
    '<toast><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="settings" placement="contextmenu"/></actions></toast>';
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL, requireInteraction: true });
  expected =
    '<toast scenario="reminder"><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="settings" placement="contextmenu"/></actions></toast>';
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));

  alert = makeAlert({ name, title, text, imageURL, actions });
  expected =
    '<toast><visual><binding template="ToastImageAndText03"><image id="1" src="file:///image.png"/><text id="1">title</text><text id="2">text</text></binding></visual><actions><action content="Notification settings" arguments="settings" placement="contextmenu"/><action content="title1" arguments="action1"/><action content="title2" arguments="action2"/></actions></toast>';
  Assert.equal(expected, alertsService.getXmlStringForWindowsAlert(alert));
});
