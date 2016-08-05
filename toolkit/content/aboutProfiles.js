/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/AppConstants.jsm');

XPCOMUtils.defineLazyServiceGetter(
  this,
  'ProfileService',
  '@mozilla.org/toolkit/profile-service;1',
  'nsIToolkitProfileService'
);

const bundle = Services.strings.createBundle(
  'chrome://global/locale/aboutProfiles.properties');

// nsIToolkitProfileService.selectProfile can be used only during the selection
// of the profile in the ProfileManager. If we are showing about:profiles in a
// tab, the selectedProfile returns the default profile.
// In this function we use the ProfD to find the current profile.
function findCurrentProfile() {
  let cpd;
  try {
    cpd = Cc["@mozilla.org/file/directory_service;1"]
            .getService(Ci.nsIProperties)
            .get("ProfD", Ci.nsIFile);
  } catch (e) {}

  if (cpd) {
    let itr = ProfileService.profiles;
    while (itr.hasMoreElements()) {
      let profile = itr.getNext().QueryInterface(Ci.nsIToolkitProfile);
      if (profile.rootDir.path == cpd.path) {
        return profile;
      }
    }
  }

  // selectedProfile can trow if nothing is selected or if the selected profile
  // has been deleted.
  try {
    return ProfileService.selectedProfile;
  } catch (e) {
    return null;
  }
}

function refreshUI() {
  let parent = document.getElementById('profiles');
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }

  let defaultProfile;
  try {
    defaultProfile = ProfileService.defaultProfile;
  } catch (e) {}

  let currentProfile = findCurrentProfile() || defaultProfile;

  let iter = ProfileService.profiles;
  while (iter.hasMoreElements()) {
    let profile = iter.getNext().QueryInterface(Ci.nsIToolkitProfile);
    display({ profile: profile,
              isDefault: profile == defaultProfile,
              isCurrentProfile: profile == currentProfile });
  }

  let createButton = document.getElementById('create-button');
  createButton.onclick = createProfileWizard;

  let restartSafeModeButton = document.getElementById('restart-in-safe-mode-button');
  restartSafeModeButton.onclick = function() { restart(true); }

  let restartNormalModeButton = document.getElementById('restart-button');
  restartNormalModeButton.onclick = function() { restart(false); }
}

function openDirectory(dir) {
  let nsLocalFile = Components.Constructor("@mozilla.org/file/local;1",
                                           "nsILocalFile", "initWithPath");
  new nsLocalFile(dir).reveal();
}

function display(profileData) {
  let parent = document.getElementById('profiles');

  let div = document.createElement('div');
  parent.appendChild(div);

  let nameStr = bundle.formatStringFromName('name', [profileData.profile.name], 1);

  let name = document.createElement('h2');
  name.appendChild(document.createTextNode(nameStr));

  div.appendChild(name);

  if (profileData.isCurrentProfile) {
    let currentProfile = document.createElement('h3');
    let currentProfileStr = bundle.GetStringFromName('currentProfile');
    currentProfile.appendChild(document.createTextNode(currentProfileStr));
    div.appendChild(currentProfile);
  }

  let table = document.createElement('table');
  div.appendChild(table);

  let tbody = document.createElement('tbody');
  table.appendChild(tbody);

  function createItem(title, value, dir = false) {
    let tr = document.createElement('tr');
    tbody.appendChild(tr);

    let th = document.createElement('th');
    th.setAttribute('class', 'column');
    th.appendChild(document.createTextNode(title));
    tr.appendChild(th);

    let td = document.createElement('td');
    td.appendChild(document.createTextNode(value));
    tr.appendChild(td);

    if (dir) {
      td.appendChild(document.createTextNode(' '));
      let button = document.createElement('button');
      let string = 'openDir';
      if (AppConstants.platform == "win") {
        string = 'winOpenDir';
      } else if (AppConstants.platform == "macosx") {
        string = 'macOpenDir';
      }
      let buttonText = document.createTextNode(bundle.GetStringFromName(string));
      button.appendChild(buttonText);
      td.appendChild(button);

      button.addEventListener('click', function(e) {
        openDirectory(value);
      });
    }
  }

  createItem(bundle.GetStringFromName('isDefault'),
             profileData.isDefault ? bundle.GetStringFromName('yes') : bundle.GetStringFromName('no'));

  createItem(bundle.GetStringFromName('rootDir'), profileData.profile.rootDir.path, true);

  if (profileData.profile.localDir.path != profileData.profile.rootDir.path) {
    createItem(bundle.GetStringFromName('localDir'), profileData.profile.localDir.path, true);
  }

  let renameButton = document.createElement('button');
  renameButton.appendChild(document.createTextNode(bundle.GetStringFromName('rename')));
  renameButton.onclick = function() {
    renameProfile(profileData.profile);
  };
  div.appendChild(renameButton);

  if (!profileData.isCurrentProfile) {
    let removeButton = document.createElement('button');
    removeButton.appendChild(document.createTextNode(bundle.GetStringFromName('remove')));
    removeButton.onclick = function() {
      removeProfile(profileData.profile);
    };

    div.appendChild(removeButton);
  }

  if (!profileData.isDefault) {
    let defaultButton = document.createElement('button');
    defaultButton.appendChild(document.createTextNode(bundle.GetStringFromName('setAsDefault')));
    defaultButton.onclick = function() {
      defaultProfile(profileData.profile);
    };
    div.appendChild(defaultButton);
  }

  if (!profileData.isCurrentProfile) {
    let runButton = document.createElement('button');
    runButton.appendChild(document.createTextNode(bundle.GetStringFromName('launchProfile')));
    runButton.onclick = function() {
      openProfile(profileData.profile);
    };
    div.appendChild(runButton);
  }

  let sep = document.createElement('hr');
  div.appendChild(sep);
}

function CreateProfile(profile) {
  ProfileService.selectedProfile = profile;
  ProfileService.flush();
  refreshUI();
}

function createProfileWizard() {
  // This should be rewritten in HTML eventually.
  window.openDialog('chrome://mozapps/content/profile/createProfileWizard.xul',
                    '', 'centerscreen,chrome,modal,titlebar',
                    ProfileService);
}

function renameProfile(profile) {
  let title = bundle.GetStringFromName('renameProfileTitle');
  let msg = bundle.formatStringFromName('renameProfile', [profile.name], 1);
  let newName = { value: profile.name };

  if (Services.prompt.prompt(window, title, msg, newName, null,
                             { value: 0 })) {
    newName = newName.value;

    if (newName == profile.name) {
      return;
    }

    try {
      profile.name = newName;
    } catch (e) {
      let title = bundle.GetStringFromName('invalidProfileNameTitle');
      let msg = bundle.formatStringFromName('invalidProfileName', [newName], 1);
      Services.prompt.alert(window, title, msg);
      return;
    }

    ProfileService.flush();
    refreshUI();
  }
}

function removeProfile(profile) {
  let deleteFiles = false;

  if (profile.rootDir.exists()) {
    let title = bundle.GetStringFromName('deleteProfileTitle');
    let msg = bundle.formatStringFromName('deleteProfileConfirm',
                                          [profile.rootDir.path], 1);

    let buttonPressed = Services.prompt.confirmEx(window, title, msg,
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0) +
                          (Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1) +
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_2),
                          bundle.GetStringFromName('dontDeleteFiles'),
                          null,
                          bundle.GetStringFromName('deleteFiles'),
                          null, {value:0});
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
    let itr = ProfileService.profiles;
    while (itr.hasMoreElements()) {
      let p = itr.getNext().QueryInterface(Ci.nsIToolkitProfile);
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

  profile.remove(deleteFiles);
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

window.addEventListener('DOMContentLoaded', function load() {
  window.removeEventListener('DOMContentLoaded', load);
  refreshUI();
});
