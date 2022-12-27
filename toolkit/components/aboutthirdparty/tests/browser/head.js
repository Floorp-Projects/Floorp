/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kClsidTestShellEx = "{10a9521e-0205-4cc7-93a1-62f30a9a54b3}";
const kFriendlyName = "Minimum Shell Extension for Firefox testing";
const kExtensionSubkeys = [".zzz\\shellex\\IconHandler"];
const kExtensionModuleName = "TestShellEx.dll";
const kFileFilterInDialog = "*.zzz";
const kATP = Cc["@mozilla.org/about-thirdparty;1"].getService(
  Ci.nsIAboutThirdParty
);

function loadShellExtension() {
  // This method call opens the file dialog and shows the support file
  // "hello.zzz" in it, which loads TestShellEx.dll to show an icon
  // for files with the .zzz extension.
  kATP.openAndCloseFileDialogForTesting(
    kExtensionModuleName,
    getTestFilePath(""),
    kFileFilterInDialog
  );
}

async function registerObject() {
  const reg = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );

  reg.create(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "Software\\Classes\\CLSID\\" + kClsidTestShellEx,
    Ci.nsIWindowsRegKey.ACCESS_ALL
  );

  reg.writeStringValue("", kFriendlyName);

  const inprocServer = reg.createChild(
    "InprocServer32",
    Ci.nsIWindowsRegKey.ACCESS_ALL
  );

  const moduleFullPath = getTestFilePath(kExtensionModuleName);
  Assert.ok(await IOUtils.exists(moduleFullPath), "The module file exists.");

  inprocServer.writeStringValue("", moduleFullPath);
  inprocServer.writeStringValue("ThreadingModel", "Apartment");
  reg.close();

  info("registerObject() done - " + moduleFullPath);
}

function registerExtensions() {
  for (const subkey of kExtensionSubkeys) {
    const reg = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    reg.create(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      "Software\\Classes\\" + subkey,
      Ci.nsIWindowsRegKey.ACCESS_ALL
    );

    let currentExtension = "";
    try {
      // If the key was just created above, the default value does not exist,
      // so readStringValue will throw NS_ERROR_FAILURE.
      currentExtension = reg.readStringValue("");
    } catch (e) {}

    try {
      if (!currentExtension) {
        reg.writeStringValue("", kClsidTestShellEx);
      } else if (currentExtension != kClsidTestShellEx) {
        throw new Error(
          `Another extension \`${currentExtension}\` has been registered.`
        );
      }
    } catch (e) {
      throw new Error("Failed to register TestShellEx.dll: " + e);
    } finally {
      reg.close();
    }
  }
}

function unregisterAll() {
  for (const subkey of kExtensionSubkeys) {
    const reg = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );

    try {
      reg.open(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        "Software\\Classes\\" + subkey,
        Ci.nsIWindowsRegKey.ACCESS_ALL
      );

      if (reg.readStringValue("") != kClsidTestShellEx) {
        // If another extension is registered, don't overwrite it.
        continue;
      }

      // Set an empty string instead of deleting the key
      // not to touch non-default values.
      reg.writeStringValue("", "");
    } catch (e) {
      info(`Failed to unregister \`${subkey}\`: ` + e);
    } finally {
      reg.close();
    }
  }

  const reg = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  reg.open(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "Software\\Classes\\CLSID",
    Ci.nsIWindowsRegKey.ACCESS_ALL
  );

  try {
    const child = reg.openChild(
      kClsidTestShellEx,
      Ci.nsIWindowsRegKey.ACCESS_ALL
    );
    try {
      child.removeChild("InprocServer32");
    } catch (e) {
    } finally {
      child.close();
    }

    reg.removeChild(kClsidTestShellEx);
  } catch (e) {
  } finally {
    reg.close();
  }
}
