/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URI_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

// This tests that the blocklist dialog still affects soft-blocked add-ons
// if the user clicks the "Restart Later" button. It also ensures that the
// "Cancel" button is correctly renamed (to "Restart Later").
var args = {
  restart: false,
  list: [{
    name: "Bug 523784 softblocked addon",
    version: "1",
    icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
    disable: false,
    blocked: false,
    url: "http://example.com/bug523784_1",
  }],
};

function test() {
  waitForExplicitFinish();

  let windowObserver = function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened")
      return;

    Services.ww.unregisterNotification(windowObserver);

    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function() {
      executeSoon(() => bug523784_test1(win));
    }, {once: true});
  };
  Services.ww.registerNotification(windowObserver);

  args.wrappedJSObject = args;
  Services.ww.openWindow(null, URI_BLOCKLIST_DIALOG, "",
                         "chrome,centerscreen,dialog,titlebar", args);
}

function bug523784_test1(win) {
  let bundle = Services.strings.
              createBundle("chrome://mozapps/locale/update/updates.properties");
  let cancelButton = win.document.documentElement.getButton("cancel");
  let moreInfoLink = win.document.getElementById("moreInfo");

  is(cancelButton.getAttribute("label"),
     bundle.GetStringFromName("restartLaterButton"),
     "Text should be changed on Cancel button");
  is(cancelButton.getAttribute("accesskey"),
     bundle.GetStringFromName("restartLaterButton.accesskey"),
     "Accesskey should also be changed on Cancel button");
  is(moreInfoLink.getAttribute("href"),
     "http://example.com/bug523784_1",
     "More Info link should link to a detailed blocklist page.");
  let windowObserver = function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowclosed")
      return;

    Services.ww.unregisterNotification(windowObserver);

    ok(args.list[0].disable, "Should be blocking add-on");
    ok(!args.restart, "Should not restart browser immediately");

    executeSoon(bug523784_test2);
  };
  Services.ww.registerNotification(windowObserver);

  cancelButton.doCommand();
}

function bug523784_test2(win) {
  let windowObserver = function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened")
      return;

    Services.ww.unregisterNotification(windowObserver);
    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function() {
      executeSoon(function() {
      let moreInfoLink = win.document.getElementById("moreInfo");
      let cancelButton = win.document.documentElement.getButton("cancel");
      is(moreInfoLink.getAttribute("href"),
         Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL"),
         "More Info link should link to the general blocklist page.");
      cancelButton.doCommand();
      executeSoon(finish);
    });
    }, {once: true});
  };
  Services.ww.registerNotification(windowObserver);

  // Add 2 more addons to the blocked list to check that the more info link
  // points to the general blocked list page.
  args.list.push({
    name: "Bug 523784 softblocked addon 2",
    version: "2",
    icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
    disable: false,
    blocked: false,
    url: "http://example.com/bug523784_2"
  });
  args.list.push({
    name: "Bug 523784 softblocked addon 3",
    version: "4",
    icon: "chrome://mozapps/skin/plugins/pluginGeneric.png",
    disable: false,
    blocked: false,
    url: "http://example.com/bug523784_3"
  });

  args.wrappedJSObject = args;
  Services.ww.openWindow(null, URI_BLOCKLIST_DIALOG, "",
                         "chrome,centerscreen,dialog,titlebar", args);
}
