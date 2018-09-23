/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

// nsIToolkitProfileService.selectProfile can be used only during the selection
// of the profile in the ProfileManager. If we are showing about:profiles in a
// tab, the selectedProfile returns the default profile.
// In this function we use the ProfD to find the current profile.
function findCurrentProfile() {
  let cpd;
  try {
    cpd = Services.dirsvc.get("ProfD", Ci.nsIFile);
  } catch (e) {}

  if (cpd) {
    for (let profile of ProfileService.profiles) {
      if (profile.rootDir.path == cpd.path) {
        return profile;
      }
    }
  }

  // selectedProfile can throw if nothing is selected or if the selected profile
  // has been deleted.
  try {
    return ProfileService.selectedProfile;
  } catch (e) {
    return null;
  }
}

function refreshUI() {
  let parent = document.getElementById("profiles");
  while (parent.firstChild) {
    parent.firstChild.remove();
  }

  let defaultProfile;
  try {
    defaultProfile = ProfileService.defaultProfile;
  } catch (e) {}

  let currentProfile = findCurrentProfile();

  for (let profile of ProfileService.profiles) {
    let isCurrentProfile = profile == currentProfile;
    let isInUse = isCurrentProfile;
    if (!isInUse) {
      try {
        let lock = profile.lock({});
        lock.unlock();
      } catch (e) {
        isInUse = true;
      }
    }
    display({
      profile,
      isDefault: profile == defaultProfile,
      isCurrentProfile,
      isInUse,
    });
  }

  let createButton = document.getElementById("create-button");
  createButton.onclick = createProfileWizard;

  let restartSafeModeButton = document.getElementById("restart-in-safe-mode-button");
  if (!Services.policies || Services.policies.isAllowed("safeMode")) {
    restartSafeModeButton.onclick = function() { restart(true); };
  } else {
    restartSafeModeButton.setAttribute("disabled", "true");
  }

  let restartNormalModeButton = document.getElementById("restart-button");
  restartNormalModeButton.onclick = function() { restart(false); };
}

function openDirectory(dir) {
  let nsLocalFile = Components.Constructor("@mozilla.org/file/local;1",
                                           "nsIFile", "initWithPath");
  new nsLocalFile(dir).reveal();
}

function display(profileData) {
  let parent = document.getElementById("profiles");

  let div = document.createElement("div");
  parent.appendChild(div);

  let name = document.createElement("h2");

  div.appendChild(name);
  document.l10n.setAttributes(name, "profiles-name", { name: profileData.profile.name });


  if (profileData.isCurrentProfile) {
    let currentProfile = document.createElement("h3");
    document.l10n.setAttributes(currentProfile, "profiles-current-profile");
    div.appendChild(currentProfile);
  } else if (profileData.isInUse) {
    let currentProfile = document.createElement("h3");
    document.l10n.setAttributes(currentProfile, "profiles-in-use-profile");
    div.appendChild(currentProfile);
  }

  let table = document.createElement("table");
  div.appendChild(table);

  let tbody = document.createElement("tbody");
  table.appendChild(tbody);

  function createItem(title, value, dir = false) {
    let tr = document.createElement("tr");
    tbody.appendChild(tr);

    let th = document.createElement("th");
    th.setAttribute("class", "column");
    document.l10n.setAttributes(th, title);
    tr.appendChild(th);

    let td = document.createElement("td");
    tr.appendChild(td);

    if (dir) {
      td.appendChild(document.createTextNode(value));
      let button = document.createElement("button");
      button.setAttribute("class", "opendir");
      document.l10n.setAttributes(button, "profiles-opendir");

      td.appendChild(button);

      button.addEventListener("click", function(e) {
        openDirectory(value);
      });
    } else {
      document.l10n.setAttributes(td, value);
    }
  }

  createItem("profiles-is-default",
    profileData.isDefault ? "profiles-yes" : "profiles-no");

  createItem("profiles-rootdir", profileData.profile.rootDir.path, true);

  if (profileData.profile.localDir.path != profileData.profile.rootDir.path) {
    createItem("profiles-localdir", profileData.profile.localDir.path, true);
  }

  let renameButton = document.createElement("button");
  document.l10n.setAttributes(renameButton, "profiles-rename");
  renameButton.onclick = function() {
    renameProfile(profileData.profile);
  };
  div.appendChild(renameButton);

  if (!profileData.isInUse) {
    let removeButton = document.createElement("button");
    document.l10n.setAttributes(removeButton, "profiles-remove");
    removeButton.onclick = function() {
      removeProfile(profileData.profile);
    };

    div.appendChild(removeButton);
  }

  if (!profileData.isDefault) {
    let defaultButton = document.createElement("button");
    document.l10n.setAttributes(defaultButton, "profiles-set-as-default");
    defaultButton.onclick = function() {
      defaultProfile(profileData.profile);
    };
    div.appendChild(defaultButton);
  }

  if (!profileData.isInUse) {
    let runButton = document.createElement("button");
    document.l10n.setAttributes(runButton, "profiles-launch-profile");
    runButton.onclick = function() {
      openProfile(profileData.profile);
    };
    div.appendChild(runButton);
  }

  let sep = document.createElement("hr");
  div.appendChild(sep);
}

function CreateProfile(profile) {
  ProfileService.selectedProfile = profile;
  ProfileService.flush();
  refreshUI();
}

function createProfileWizard() {
  // This should be rewritten in HTML eventually.
  window.openDialog("chrome://mozapps/content/profile/createProfileWizard.xul",
                    "", "centerscreen,chrome,modal,titlebar",
                    ProfileService);
}

async function renameProfile(profile) {

  let newName = { value: profile.name };
  let [title, msg] = await document.l10n.formatValues([
    { id: "profiles-rename-profile-title" },
    { id: "profiles-rename-profile", args: { name: profile.name } },
  ]);

  if (Services.prompt.prompt(window, title, msg, newName, null,
                             { value: 0 })) {
    newName = newName.value;

    if (newName == profile.name) {
      return;
    }

    try {
      profile.name = newName;
    } catch (e) {
      let [title, msg] = await document.l10n.formatValues([
        { id: "profiles-invalid-profile-name-title" },
        { id: "profiles-invalid-profile-name", args: { name: newName } },
      ]);

      Services.prompt.alert(window, title, msg);
      return;
    }

    ProfileService.flush();
    refreshUI();
  }
}

async function removeProfile(profile) {
  let deleteFiles = false;

  if (profile.rootDir.exists()) {
    let [title, msg, dontDeleteStr, deleteStr] = await document.l10n.formatValues([
      { id: "profiles-delete-profile-title" },
      { id: "profiles-delete-profile-confirm", args: { dir: profile.rootDir.path } },
      { id: "profiles-dont-delete-files" },
      { id: "profiles-delete-files" },
    ]);
    let buttonPressed = Services.prompt.confirmEx(window, title, msg,
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0) +
                          (Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1) +
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_2),
                          dontDeleteStr,
                          null,
                          deleteStr,
                          null, {value: 0});
    if (buttonPressed == 1) {
      return;
    }

    if (buttonPressed == 2) {
      deleteFiles = true;
    }
  }

  // If we are deleting the selected or the default profile we must choose a
  // different one.
  let isSelected = false;
  try {
    isSelected = ProfileService.selectedProfile == profile;
  } catch (e) {}

  let isDefault = false;
  try {
    isDefault = ProfileService.defaultProfile == profile;
  } catch (e) {}

  if (isSelected || isDefault) {
    for (let p of ProfileService.profiles) {
      if (profile == p) {
        continue;
      }

      if (isSelected) {
        ProfileService.selectedProfile = p;
      }

      if (isDefault) {
        ProfileService.defaultProfile = p;
      }

      break;
    }
  }

  try {
    profile.removeInBackground(deleteFiles);
  } catch (e) {
    let [title, msg] = await document.l10n.formatValues([
        { id: "profiles-delete-profile-failed-title" },
        { id: "profiles-delete-profile-failed-message" },
    ]);

    Services.prompt.alert(window, title, msg);
    return;
  }

  ProfileService.flush();
  refreshUI();
}

function defaultProfile(profile) {
  ProfileService.defaultProfile = profile;
  ProfileService.selectedProfile = profile;
  ProfileService.flush();
  refreshUI();
}

function openProfile(profile) {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

  if (cancelQuit.data) {
    return;
  }

  Services.startup.createInstanceWithProfile(profile);
}

function restart(safeMode) {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

  if (cancelQuit.data) {
    return;
  }

  let flags = Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestartNotSameProfile;

  if (safeMode) {
    Services.startup.restartInSafeMode(flags);
  } else {
    Services.startup.quit(flags);
  }
}

window.addEventListener("DOMContentLoaded", function() {
  refreshUI();
}, {once: true});
