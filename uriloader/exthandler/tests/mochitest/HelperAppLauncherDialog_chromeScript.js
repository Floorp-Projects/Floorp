/* eslint-env mozilla/chrome-script */

const { ComponentUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ComponentUtils.sys.mjs"
);

const { Downloads } = ChromeUtils.importESModule(
  "resource://gre/modules/Downloads.sys.mjs"
);

let gMIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

const HELPERAPP_DIALOG_CONTRACT = "@mozilla.org/helperapplauncherdialog;1";
const HELPERAPP_DIALOG_CID = Components.ID(
  Cc[HELPERAPP_DIALOG_CONTRACT].number
);

let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
tmpDir.append("testsavedir" + Math.floor(Math.random() * 2 ** 32));
// Create this dir if it doesn't exist (ignores existing dirs)
try {
  tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o777, true);
} catch (ex) {
  if (ex.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
    throw ex;
  }
}
Services.prefs.setIntPref("browser.download.folderList", 2);
Services.prefs.setCharPref("browser.download.dir", tmpDir.path);

const FAKE_CID = Services.uuid.generateUUID();
function HelperAppLauncherDialog() {}
HelperAppLauncherDialog.prototype = {
  show(aLauncher) {
    if (
      Services.prefs.getBoolPref(
        "browser.download.always_ask_before_handling_new_types"
      )
    ) {
      let f = tmpDir.clone();
      f.append(aLauncher.suggestedFileName);
      aLauncher.saveDestinationAvailable(f);
      sendAsyncMessage("suggestedFileName", aLauncher.suggestedFileName);
    } else {
      sendAsyncMessage("wrongAPICall", "show");
    }
    aLauncher.cancel(Cr.NS_BINDING_ABORTED);
  },
  promptForSaveToFileAsync(appLauncher, parent, filename) {
    if (
      !Services.prefs.getBoolPref(
        "browser.download.always_ask_before_handling_new_types"
      )
    ) {
      let f = tmpDir.clone();
      f.append(filename);
      appLauncher.saveDestinationAvailable(f);
      sendAsyncMessage("suggestedFileName", filename);
    } else {
      sendAsyncMessage("wrongAPICall", "promptForSaveToFileAsync");
    }
    appLauncher.cancel(Cr.NS_BINDING_ABORTED);
  },
  QueryInterface: ChromeUtils.generateQI(["nsIHelperAppLauncherDialog"]),
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(
  FAKE_CID,
  "",
  HELPERAPP_DIALOG_CONTRACT,
  ComponentUtils.generateSingletonFactory(HelperAppLauncherDialog)
);

addMessageListener("unregister", async function () {
  registrar.registerFactory(
    HELPERAPP_DIALOG_CID,
    "",
    HELPERAPP_DIALOG_CONTRACT,
    null
  );
  let list = await Downloads.getList(Downloads.ALL);
  let downloads = await list.getAll();
  for (let dl of downloads) {
    await dl.refresh();
    if (dl.target.exists || dl.target.partFileExists) {
      dump("Finalizing download.\n");
      await dl.finalize(true).catch(console.error);
    }
  }
  await list.removeFinished();
  dump("Clearing " + tmpDir.path + "\n");
  tmpDir.remove(true);
  sendAsyncMessage("unregistered");
});
