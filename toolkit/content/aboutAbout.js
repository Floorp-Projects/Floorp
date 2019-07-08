/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AboutPagesUtils } = ChromeUtils.import(
  "resource://gre/modules/AboutPagesUtils.jsm"
);

var gContainer;
window.onload = function() {
  gContainer = document.getElementById("abouts");
  AboutPagesUtils.visibleAboutUrls.forEach(createProtocolListing);
};

function createProtocolListing(aUrl) {
  var li = document.createElement("li");
  var link = document.createElement("a");
  var text = document.createTextNode(aUrl);

  link.href = aUrl;
  link.appendChild(text);
  li.appendChild(link);
  gContainer.appendChild(li);
}
