// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global MozXULElement */
/* exported init, finish */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var gArgs;

function init() {
  var hasHardBlocks = false;
  var hasSoftBlocks = false;
  gArgs = window.arguments[0].wrappedJSObject;

  document.addEventListener("dialogaccept", function() { finish(true); });
  document.addEventListener("dialogcancel", function() { finish(false); });

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
  list.sort((a, b) => String(a.name).localeCompare(b.name));
  for (let listItem of list) {
    let item = document.createXULElement("richlistitem");

    const icon = document.createXULElement("image");
    icon.src = listItem.icon;

    const container = document.createXULElement("vbox");
    container.flex = 1;

    const nameVersion = document.createXULElement("hbox");
    nameVersion.className = "addon-name-version";

    const name = document.createXULElement("label");
    name.className = "addonName";
    name.value = listItem.name;
    name.crop = "end";
    const version = document.createXULElement("label");
    version.value = listItem.version;

    nameVersion.append(name, version);

    const fragment = document.createXULElement("hbox");
    fragment.pack = "end";

    if (listItem.blocked) {
      fragment.appendChild(MozXULElement.parseXULToFragment(`
        <label class="blockedLabel" value="&blocklist.blocked.label;"/>
      `, ["chrome://mozapps/locale/extensions/blocklist.dtd"]));
      hasHardBlocks = true;
    } else {
      fragment.appendChild(MozXULElement.parseXULToFragment(`
        <checkbox class="disableCheckbox" checked="true" label="&blocklist.checkbox.label;"/>
      `, ["chrome://mozapps/locale/extensions/blocklist.dtd"]));
      hasSoftBlocks = true;
    }

    container.append(nameVersion, fragment);
    item.append(icon, container);
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
  } else {
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
      list[i].disable = items[i].querySelector(".disableCheckbox").checked;
  }
}
