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
# The Original Code is the Extension Blocklist UI.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation
# Portions created by the Initial Developer are Copyright (C) 2008
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

Components.utils.import("resource://gre/modules/Services.jsm");

var gArgs;

function init() {
  var hasHardBlocks = false;
  var hasSoftBlocks = false;
  gArgs = window.arguments[0].wrappedJSObject;

  // NOTE: We use strings from the "updates.properties" bundleset to change the
  // text on the "Cancel" button to "Restart Later". (bug 523784)
  let bundle = Services.strings.
              createBundle("chrome://mozapps/locale/update/updates.properties");
  let cancelButton = document.documentElement.getButton("cancel");
  cancelButton.setAttribute("label", bundle.GetStringFromName("restartLaterButton"));
  cancelButton.setAttribute("accesskey",
                            bundle.GetStringFromName("restartLaterButton.accesskey"));

  var richlist = document.getElementById("addonList");
  var list = gArgs.list;
  list.sort(function(a, b) { return String.localeCompare(a.name, b.name); });
  for (let i = 0; i < list.length; i++) {
    let item = document.createElement("richlistitem");
    item.setAttribute("name", list[i].name);
    item.setAttribute("version", list[i].version);
    item.setAttribute("icon", list[i].icon);
    if (list[i].blocked) {
      item.setAttribute("class", "hardBlockedAddon");
      hasHardBlocks = true;
    }
    else {
      item.setAttribute("class", "softBlockedAddon");
      hasSoftBlocks = true;
    }
    richlist.appendChild(item);
  }

  if (hasHardBlocks && hasSoftBlocks)
    document.getElementById("bothMessage").hidden = false;
  else if (hasHardBlocks)
    document.getElementById("hardBlockMessage").hidden = false;
  else
    document.getElementById("softBlockMessage").hidden = false;

  var link = document.getElementById("moreInfo");
  if (list.length == 1 && list[0].url) {
    link.setAttribute("href", list[0].url);
  }
  else {
    var url = Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL");
    link.setAttribute("href", url);
  }
}

function finish(shouldRestartNow) {
  gArgs.restart = shouldRestartNow;
  var list = gArgs.list;
  var items = document.getElementById("addonList").childNodes;
  for (let i = 0; i < list.length; i++) {
    if (!list[i].blocked)
      list[i].disable = items[i].checked;
  }
  return true;
}
