/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const C = Cc;
const I = Ci;

const ToolkitProfileService = "@mozilla.org/toolkit/profile-service;1";

const fluentStrings = new Localization([
  "branding/brand.ftl",
  "toolkit/global/profileSelection.ftl",
]);

var gDialogParams;
var gProfileManagerBundle;
var gBrandBundle;
var gProfileService;
var gNeedsFlush = false;

function getFluentString(str) {
  return fluentStrings.formatValue(str);
}

function startup() {
  try {
    gDialogParams = window.arguments[0].QueryInterface(I.nsIDialogParamBlock);

    gProfileService = C[ToolkitProfileService].getService(
      I.nsIToolkitProfileService
    );

    gProfileManagerBundle = document.getElementById("bundle_profileManager");
    gBrandBundle = document.getElementById("bundle_brand");

    document.getElementById("profileWindow").centerWindowOnScreen();

    var profilesElement = document.getElementById("profiles");

    for (let profile of gProfileService.profiles.entries(I.nsIToolkitProfile)) {
      var listitem = profilesElement.appendItem(profile.name, "");

      var tooltiptext = gProfileManagerBundle.getFormattedString(
        "profileTooltip",
        [profile.name, profile.rootDir.path]
      );
      listitem.setAttribute("tooltiptext", tooltiptext);
      listitem.profile = profile;
      try {
        if (profile === gProfileService.defaultProfile) {
          setTimeout(
            function(a) {
              profilesElement.ensureElementIsVisible(a);
              profilesElement.selectItem(a);
            },
            0,
            listitem
          );
        }
      } catch (e) {}
    }

    var autoSelectLastProfile = document.getElementById(
      "autoSelectLastProfile"
    );
    autoSelectLastProfile.checked = gProfileService.startWithLastProfile;
    profilesElement.focus();
  } catch (e) {
    window.close();
    throw e;
  }
  document.addEventListener("dialogaccept", acceptDialog);
  document.addEventListener("dialogcancel", exitDialog);
}

async function flush(cancelled) {
  updateStartupPrefs();

  gDialogParams.SetInt(
    1,
    document.getElementById("offlineState").checked ? 1 : 0
  );

  if (gNeedsFlush) {
    try {
      gProfileService.flush();
    } catch (e) {
      let appName = gBrandBundle.getString("brandShortName");

      let title = gProfileManagerBundle.getString("flushFailTitle");
      let restartButton = gProfileManagerBundle.getFormattedString(
        "flushFailRestartButton",
        [appName]
      );
      let exitButton = gProfileManagerBundle.getString("flushFailExitButton");

      let message;
      if (e.result == undefined) {
        message = await getFluentString("profile-selection-conflict-message");
      } else {
        message = gProfileManagerBundle.getString("flushFailMessage");
      }

      const PS = Ci.nsIPromptService;
      let result = Services.prompt.confirmEx(
        window,
        title,
        message,
        PS.BUTTON_POS_0 * PS.BUTTON_TITLE_IS_STRING +
          PS.BUTTON_POS_1 * PS.BUTTON_TITLE_IS_STRING,
        restartButton,
        exitButton,
        null,
        null,
        {}
      );

      gDialogParams.SetInt(
        0,
        result == 0
          ? Ci.nsIToolkitProfileService.restart
          : Ci.nsIToolkitProfileService.exit
      );
      return;
    }
    gNeedsFlush = false;
  }

  gDialogParams.SetInt(
    0,
    cancelled
      ? Ci.nsIToolkitProfileService.exit
      : Ci.nsIToolkitProfileService.launchWithProfile
  );
}

function acceptDialog(event) {
  var appName = gBrandBundle.getString("brandShortName");

  var profilesElement = document.getElementById("profiles");
  var selectedProfile = profilesElement.selectedItem;
  if (!selectedProfile) {
    var pleaseSelectTitle = gProfileManagerBundle.getString(
      "pleaseSelectTitle"
    );
    var pleaseSelect = gProfileManagerBundle.getFormattedString(
      "pleaseSelect",
      [appName]
    );
    Services.prompt.alert(window, pleaseSelectTitle, pleaseSelect);
    event.preventDefault();
    return;
  }

  gDialogParams.objects.insertElementAt(selectedProfile.profile.rootDir, 0);
  gDialogParams.objects.insertElementAt(selectedProfile.profile.localDir, 1);

  if (gProfileService.defaultProfile != selectedProfile.profile) {
    try {
      gProfileService.defaultProfile = selectedProfile.profile;
      gNeedsFlush = true;
    } catch (e) {
      // This can happen on dev-edition. We'll still restart with the selected
      // profile based on the lock's directories.
    }
  }
  flush(false);
}

function exitDialog() {
  flush(true);
}

function updateStartupPrefs() {
  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  if (gProfileService.startWithLastProfile != autoSelectLastProfile.checked) {
    gProfileService.startWithLastProfile = autoSelectLastProfile.checked;
    gNeedsFlush = true;
  }
}

// handle key event on listboxes
function onProfilesKey(aEvent) {
  switch (aEvent.keyCode) {
    case KeyEvent.DOM_VK_BACK_SPACE:
      if (AppConstants.platform != "macosx") {
        break;
      }
    // fall through
    case KeyEvent.DOM_VK_DELETE:
      ConfirmDelete();
      break;
    case KeyEvent.DOM_VK_F2:
      RenameProfile();
      break;
  }
}

function onProfilesDblClick(aEvent) {
  if (aEvent.target.closest("richlistitem")) {
    document.getElementById("profileWindow").acceptDialog();
  }
}

// invoke the createProfile Wizard
function CreateProfileWizard() {
  window.openDialog(
    "chrome://mozapps/content/profile/createProfileWizard.xhtml",
    "",
    "centerscreen,chrome,modal,titlebar",
    gProfileService,
    { CreateProfile }
  );
}

/**
 * Called from createProfileWizard to update the display.
 */
function CreateProfile(aProfile) {
  var profilesElement = document.getElementById("profiles");

  var listitem = profilesElement.appendItem(aProfile.name, "");

  var tooltiptext = gProfileManagerBundle.getFormattedString("profileTooltip", [
    aProfile.name,
    aProfile.rootDir.path,
  ]);
  listitem.setAttribute("tooltiptext", tooltiptext);
  listitem.profile = aProfile;

  profilesElement.ensureElementIsVisible(listitem);
  profilesElement.selectItem(listitem);

  gNeedsFlush = true;
}

// rename the selected profile
function RenameProfile() {
  var profilesElement = document.getElementById("profiles");
  var selectedItem = profilesElement.selectedItem;
  if (!selectedItem) {
    return false;
  }

  var selectedProfile = selectedItem.profile;

  var oldName = selectedProfile.name;
  var newName = { value: oldName };

  var dialogTitle = gProfileManagerBundle.getString("renameProfileTitle");
  var msg = gProfileManagerBundle.getFormattedString("renameProfilePrompt", [
    oldName,
  ]);

  if (
    Services.prompt.prompt(window, dialogTitle, msg, newName, null, {
      value: 0,
    })
  ) {
    newName = newName.value;

    // User hasn't changed the profile name. Treat as if cancel was pressed.
    if (newName == oldName) {
      return false;
    }

    try {
      selectedProfile.name = newName;
      gNeedsFlush = true;
    } catch (e) {
      var alTitle = gProfileManagerBundle.getString("profileNameInvalidTitle");
      var alMsg = gProfileManagerBundle.getFormattedString(
        "profileNameInvalid",
        [newName]
      );
      Services.prompt.alert(window, alTitle, alMsg);
      return false;
    }

    selectedItem.firstChild.setAttribute("value", newName);
    var tiptext = gProfileManagerBundle.getFormattedString("profileTooltip", [
      newName,
      selectedProfile.rootDir.path,
    ]);
    selectedItem.setAttribute("tooltiptext", tiptext);

    return true;
  }

  return false;
}

function ConfirmDelete() {
  var profileList = document.getElementById("profiles");

  var selectedItem = profileList.selectedItem;
  if (!selectedItem) {
    return false;
  }

  var selectedProfile = selectedItem.profile;
  var deleteFiles = false;

  if (selectedProfile.rootDir.exists()) {
    var dialogTitle = gProfileManagerBundle.getString("deleteTitle");
    var dialogText = gProfileManagerBundle.getFormattedString(
      "deleteProfileConfirm",
      [selectedProfile.rootDir.path]
    );

    var buttonPressed = Services.prompt.confirmEx(
      window,
      dialogTitle,
      dialogText,
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
        Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
        Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_2,
      gProfileManagerBundle.getString("dontDeleteFiles"),
      null,
      gProfileManagerBundle.getString("deleteFiles"),
      null,
      { value: 0 }
    );
    if (buttonPressed == 1) {
      return false;
    }

    if (buttonPressed == 2) {
      deleteFiles = true;
    }
  }

  try {
    selectedProfile.remove(deleteFiles);
    gNeedsFlush = true;
  } catch (e) {
    let title = gProfileManagerBundle.getString("profileDeletionFailedTitle");
    let msg = gProfileManagerBundle.getString("profileDeletionFailed");
    Services.prompt.alert(window, title, msg);

    return true;
  }

  profileList.removeChild(selectedItem);
  if (profileList.firstChild != undefined) {
    profileList.selectItem(profileList.firstChild);
  }

  return true;
}
