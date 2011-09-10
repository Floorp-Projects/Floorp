# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is The Extension Update Service.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"use strict";

Components.utils.import("resource://gre/modules/AddonManager.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

var gView = null;

function showView(aView) {
  gView = aView;

  gView.show();

  // If the view's show method immediately showed a different view then don't
  // do anything else
  if (gView != aView)
    return;

  let viewNode = document.getElementById(gView.nodeID);
  viewNode.parentNode.selectedPanel = viewNode;

  // For testing dispatch an event when the view changes
  var event = document.createEvent("Events");
  event.initEvent("ViewChanged", true, true);
  viewNode.dispatchEvent(event);
}

function showButtons(aCancel, aBack, aNext, aDone) {
  document.getElementById("cancel").hidden = !aCancel;
  document.getElementById("back").hidden = !aBack;
  document.getElementById("next").hidden = !aNext;
  document.getElementById("done").hidden = !aDone;
}

function orderForScope(aScope) {
  switch (aScope) {
  case AddonManager.SCOPE_PROFILE:
    return 2;
  case AddonManager.SCOPE_APPLICATION:
    return 1;
  default:
    return 0;
  }
}

var gAddons = {};

var gChecking = {
  nodeID: "checking",

  _progress: null,
  _addonCount: 0,
  _completeCount: 0,

  show: function() {
    showButtons(true, false, false, false);
    this._progress = document.getElementById("checking-progress");

    let self = this;
    AddonManager.getAllAddons(function(aAddons) {
      if (aAddons.length == 0) {
        window.close();
        return;
      }

      aAddons = aAddons.filter(function(aAddon) {
        if (aAddon.type == "plugin")
          return false;

        if (aAddon.type == "theme") {
          // Don't show application shipped themes
          if (aAddon.scope == AddonManager.SCOPE_APPLICATION)
            return false;
          // Don't show already disabled themes
          if (aAddon.userDisabled)
            return false;
        }

        return true;
      });

      self._addonCount = aAddons.length;
      self._progress.value = 0;
      self._progress.max = aAddons.length;
      self._progress.mode = "determined";

      aAddons.forEach(function(aAddon) {
        // Ignore disabled themes
        if (aAddon.type != "theme" || !aAddon.userDisabled) {
          gAddons[aAddon.id] = {
            addon: aAddon,
            install: null,
            wasActive: aAddon.isActive
          }
        }

        aAddon.findUpdates(self, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
      });
    });
  },

  onUpdateAvailable: function(aAddon, aInstall) {
    // If the add-on can be upgraded then remember the new version
    if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE)
      gAddons[aAddon.id].install = aInstall;
  },

  onUpdateFinished: function(aAddon, aError) {
    this._completeCount++;
    this._progress.value = this._completeCount;

    if (this._completeCount < this._addonCount)
      return;

    var addons = [gAddons[id] for (id in gAddons)];

    addons.sort(function(a, b) {
      let orderA = orderForScope(a.addon.scope);
      let orderB = orderForScope(b.addon.scope);

      if (orderA != orderB)
        return orderA - orderB;

      return String.localeCompare(a.addon.name, b.addon.name);
    });

    let rows = document.getElementById("select-rows");
    let lastAddon = null;
    addons.forEach(function(aEntry) {
      if (lastAddon &&
          orderForScope(aEntry.addon.scope) != orderForScope(lastAddon.scope)) {
        let separator = document.createElement("separator");
        rows.appendChild(separator);
      }

      let row = document.createElement("row");
      row.setAttribute("id", aEntry.addon.id);
      row.setAttribute("class", "addon");
      rows.appendChild(row);
      row.setAddon(aEntry.addon, aEntry.install, aEntry.wasActive);

      if (aEntry.install)
        aEntry.install.addListener(gUpdate);

      lastAddon = aEntry.addon;
    });

    showView(gSelect);
  }
};

var gSelect = {
  nodeID: "select",

  show: function() {
    this.updateButtons();
  },

  updateButtons: function() {
    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName == "separator")
        continue;

      if (row.action) {
        showButtons(false, false, true, false);
        return;
      }
    }

    showButtons(false, false, false, true);
  },

  next: function() {
    showView(gConfirm);
  },

  done: function() {
    window.close();
  }
};

var gConfirm = {
  nodeID: "confirm",

  show: function() {
    showButtons(false, true, false, true);

    let box = document.getElementById("confirm-scrollbox").firstChild;
    while (box) {
      box.hidden = true;
      while (box.lastChild != box.firstChild)
        box.removeChild(box.lastChild);
      box = box.nextSibling;
    }

    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName == "separator")
        continue;

      let action = row.action;
      if (!action)
        continue;

      let list = document.getElementById(action + "-list");

      list.hidden = false;
      let item = document.createElement("hbox");
      item.setAttribute("id", row._addon.id);
      item.setAttribute("class", "addon");
      item.setAttribute("type", row._addon.type);
      item.setAttribute("name", row._addon.name);
      if (action == "update" || action == "enable")
        item.setAttribute("active", "true");
      list.appendChild(item);

      if (action == "update")
        showButtons(false, true, true, false);
    }
  },

  back: function() {
    showView(gSelect);
  },

  next: function() {
    showView(gUpdate);
  },

  done: function() {
    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName != "separator")
        row.apply();
    }

    window.close();
  }
}

var gUpdate = {
  nodeID: "update",

  _progress: null,
  _waitingCount: 0,
  _completeCount: 0,
  _errorCount: 0,

  show: function() {
    showButtons(true, false, false, false);

    this._progress = document.getElementById("update-progress");

    for (let row = document.getElementById("select-rows").firstChild;
         row; row = row.nextSibling) {
      if (row.localName != "separator")
        row.apply();
    }

    this._progress.mode = "determined";
    this._progress.max = this._waitingCount;
    this._progress.value = this._completeCount;
  },

  checkComplete: function() {
    this._progress.value = this._completeCount;
    if (this._completeCount < this._waitingCount)
      return;

    if (this._errorCount > 0) {
      showView(gErrors);
      return;
    }

    window.close();
  },

  onDownloadStarted: function(aInstall) {
    this._waitingCount++;
  },

  onDownloadFailed: function(aInstall) {
    this._errorCount++;
    this._completeCount++;
    this.checkComplete();
  },

  onInstallFailed: function(aInstall) {
    this._errorCount++;
    this._completeCount++;
    this.checkComplete();
  },

  onInstallEnded: function(aInstall) {
    this._completeCount++;
    this.checkComplete();
  }
};

var gErrors = {
  nodeID: "errors",

  show: function() {
    showButtons(false, false, false, true);
  },

  done: function() {
    window.close();
  }
};

window.addEventListener("load", function() { showView(gChecking); }, false);

// When closing the window cancel any pending or in-progress installs
window.addEventListener("unload", function() {
  for (let id in gAddons) {
    let entry = gAddons[id];
    if (!entry.install)
      return;

    aEntry.install.removeListener(gUpdate);

    if (entry.install.state != AddonManager.STATE_INSTALLED &&
        entry.install.state != AddonManager.STATE_DOWNLOAD_FAILED &&
        entry.install.state != AddonManager.STATE_INSTALL_FAILED) {
      entry.install.cancel();
    }
  }
}, false);
