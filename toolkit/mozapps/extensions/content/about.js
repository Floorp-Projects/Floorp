// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function init() {
  var addon = window.arguments[0];
  var extensionsStrings = document.getElementById("extensionsStrings");

  document.documentElement.setAttribute("addontype", addon.type);

  if (addon.iconURL) {
    var extensionIcon = document.getElementById("extensionIcon");
    extensionIcon.src = addon.iconURL;
  }

  document.title = extensionsStrings.getFormattedString("aboutWindowTitle", [addon.name]);
  var extensionName = document.getElementById("extensionName");
  extensionName.textContent = addon.name;

  var extensionVersion = document.getElementById("extensionVersion");
  if (addon.version)
    extensionVersion.setAttribute("value", extensionsStrings.getFormattedString("aboutWindowVersionString", [addon.version]));
  else
    extensionVersion.hidden = true;

  var extensionDescription = document.getElementById("extensionDescription");
  if (addon.description)
    extensionDescription.textContent = addon.description;
  else
    extensionDescription.hidden = true;

  var numDetails = 0;

  var extensionCreator = document.getElementById("extensionCreator");
  if (addon.creator) {
    extensionCreator.setAttribute("value", addon.creator);
    numDetails++;
  } else {
    extensionCreator.hidden = true;
    var extensionCreatorLabel = document.getElementById("extensionCreatorLabel");
    extensionCreatorLabel.hidden = true;
  }

  var extensionHomepage = document.getElementById("extensionHomepage");
  var homepageURL = addon.homepageURL;
  if (homepageURL) {
    extensionHomepage.setAttribute("homepageURL", homepageURL);
    extensionHomepage.setAttribute("tooltiptext", homepageURL);
    numDetails++;
  } else {
    extensionHomepage.hidden = true;
  }

  numDetails += appendToList("extensionDevelopers", "developersBox", addon.developers);
  numDetails += appendToList("extensionTranslators", "translatorsBox", addon.translators);
  numDetails += appendToList("extensionContributors", "contributorsBox", addon.contributors);

  if (numDetails == 0) {
    var groove = document.getElementById("groove");
    groove.hidden = true;
    var extensionDetailsBox = document.getElementById("extensionDetailsBox");
    extensionDetailsBox.hidden = true;
  }

  var acceptButton = document.documentElement.getButton("accept");
  acceptButton.label = extensionsStrings.getString("aboutWindowCloseButton");
  
  setTimeout(sizeToContent, 0);
}

function appendToList(aHeaderId, aNodeId, aItems) {
  var header = document.getElementById(aHeaderId);
  var node = document.getElementById(aNodeId);

  if (!aItems || aItems.length == 0) {
    header.hidden = true;
    return 0;
  }

  for (let currentItem of aItems) {
    var label = document.createElement("label");
    label.textContent = currentItem;
    label.setAttribute("class", "contributor");
    node.appendChild(label);
  }

  return aItems.length;
}

function loadHomepage(aEvent) {
  window.close();
  openURL(aEvent.target.getAttribute("homepageURL"));
}
