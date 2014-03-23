Cu.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();

  let mutObserverAlarmSet = null;
  let mutObserverAlarmFired = null;

  let alarmSet = false;

  loadWebapp("alarm.webapp", undefined, function onLoad() {
    let principal = document.getElementById("content").contentDocument.defaultView.document.nodePrincipal;
    let permValue = Services.perms.testExactPermissionFromPrincipal(principal, "alarms");
    is(permValue, Ci.nsIPermissionManager.ALLOW_ACTION, "Alarm permission: allow.");

    let msgSet = gAppBrowser.contentDocument.getElementById("msgSet");
    mutObserverAlarmSet = new MutationObserver(function(mutations) {
      is(msgSet.textContent, "Success.", "Alarm added.");
      alarmSet = true;
    });
    mutObserverAlarmSet.observe(msgSet, { childList: true });

    let msgFired = gAppBrowser.contentDocument.getElementById("msgFired");
    mutObserverAlarmFired = new MutationObserver(function(mutations) {
      is(msgFired.textContent, "Alarm fired.", "Alarm fired.");

      ok(alarmSet, "Alarm set before firing.");

      finish();
    });
    mutObserverAlarmFired.observe(msgFired, { childList: true });
  });

  registerCleanupFunction(function() {
    mutObserverAlarmSet.disconnect();
    mutObserverAlarmFired.disconnect();
  });
}
