/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests bug 567127 - Add install button to the add-ons manager

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

var gManagerWindow;

function checkInstallConfirmation(...urls) {
  return new Promise(resolve => {
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

        if (windows.size > 0) {
          return;
        }

        is(urls.length, 0, "Saw install dialogs for all expected urls");

        Services.wm.removeListener(listener);

        is(notificationCount, nurls, `Saw ${nurls} addon-install-started notifications`);
        Services.obs.removeObserver(observer, "addon-install-started");

        resolve();
      }
    };

    Services.wm.addListener(listener);
  });
}

add_task(async function test_install_from_file() {
  gManagerWindow = await open_manager("addons://list/extension");

  var filePaths = [
                   get_addon_file_url("browser_bug567127_1.xpi"),
                   get_addon_file_url("browser_bug567127_2.xpi")
                  ];
  MockFilePicker.setFiles(filePaths.map(aPath => aPath.file));

  // Set handler that executes the core test after the window opens,
  // and resolves the promise when the window closes
  let pInstallURIClosed = checkInstallConfirmation(...filePaths.map(path => path.spec));

  gManagerWindow.gViewController.doCommand("cmd_installFromFile");

  await pInstallURIClosed;

  MockFilePicker.cleanup();
  await close_manager(gManagerWindow);
});
