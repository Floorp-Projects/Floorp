/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests simulated drag and drop of files into the add-ons manager.
// We test with the add-ons manager in its own tab if in Firefox otherwise
// in its own window.
// Tests are only simulations of the drag and drop events, we cannot really do
// this automatically.

// Instead of loading EventUtils.js into the test scope in browser-test.js for all tests,
// we only need EventUtils.js for a few files which is why we are using loadSubScript.
var gManagerWindow;
var EventUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

function checkInstallConfirmation(...urls) {
  let nurls = urls.length;

  let notificationCount = 0;
  let observer = {
    observe(aSubject, aTopic, aData) {
      var installInfo = aSubject.wrappedJSObject;
      isnot(installInfo.browser, null, "Notification should have non-null browser");
      notificationCount++;
    }
  };
  Services.obs.addObserver(observer, "addon-install-started");

  let windows = new Set();

  function handleDialog(window) {
    let list = window.document.getElementById("itemList");
    is(list.childNodes.length, 1, "Should be 1 install");
    let idx = urls.indexOf(list.children[0].url);
    isnot(idx, -1, "Install target is an expected url");
    urls.splice(idx, 1);

    window.document.documentElement.cancelDialog();
  }

  let listener = {
    handleEvent(event) {
      let window = event.currentTarget;
      is(window.document.location.href, INSTALL_URI, "Should have opened the correct window");

      executeSoon(() => handleDialog(window));
    },

    onWindowTitleChange() { },

    onOpenWindow(window) {
      windows.add(window);
      let domwindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow);
      domwindow.addEventListener("load", this, false, {once: true});
    },

    onCloseWindow(window) {
      if (!windows.has(window)) {
        return;
      }
      windows.delete(window);

      if (windows.size > 0 || urls.length > 0) {
        return;
      }

      Services.wm.removeListener(listener);

      is(notificationCount, nurls, `Saw ${nurls} addon-install-started notifications`);
      Services.obs.removeObserver(observer, "addon-install-started");

      executeSoon(run_next_test);
    }
  };

  Services.wm.addListener(listener);
}

function test() {
  waitForExplicitFinish();

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

// Simulates dropping a URL onto the manager
add_test(function() {
  var url = TESTROOT + "addons/browser_dragdrop1.xpi";

  checkInstallConfirmation(url);

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping a file onto the manager
add_test(function() {
  var fileurl = get_addon_file_url("browser_dragdrop1.xpi");

  checkInstallConfirmation(fileurl.spec);

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "application/x-moz-file", data: fileurl.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping two urls onto the manager
add_test(function() {
  var url1 = TESTROOT + "addons/browser_dragdrop1.xpi";
  var url2 = TESTROOT2 + "addons/browser_dragdrop2.xpi";

  checkInstallConfirmation(url1, url2);

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url1}],
                                          [{type: "text/x-moz-url", data: url2}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping two files onto the manager
add_test(function() {
  var fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
  var fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");

  checkInstallConfirmation(fileurl1.spec, fileurl2.spec);

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "application/x-moz-file", data: fileurl1.file}],
                                          [{type: "application/x-moz-file", data: fileurl2.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping a file and a url onto the manager (weird, but should still work)
add_test(function() {
  var url = TESTROOT + "addons/browser_dragdrop1.xpi";
  var fileurl = get_addon_file_url("browser_dragdrop2.xpi");

  checkInstallConfirmation(url, fileurl.spec);

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url}],
                                          [{type: "application/x-moz-file", data: fileurl.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});
