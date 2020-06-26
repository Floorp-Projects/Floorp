/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

const searchParams = new URLSearchParams(document.documentURI.split("?")[1]);

function initPage() {
  if (!searchParams.get("e")) {
    document.getElementById("error").remove();
  }

  const explanation1 = document.getElementById(
    "insecure-explanation-unavailable"
  );

  const pageUrl = new URL(window.location.href.replace(/^view-source:/, ""));

  document.l10n.setAttributes(
    explanation1,
    "about-httpsonly-explanation-unavailable",
    { websiteUrl: pageUrl.host }
  );

  document
    .getElementById("openInsecure")
    .addEventListener("click", onOpenInsecureButtonClick);

  if (window.top == window) {
    document
      .getElementById("goBack")
      .addEventListener("click", onReturnButtonClick);
    addAutofocus("#goBack", "beforeend");
  } else {
    document.getElementById("goBack").remove();
  }
}

/*  Button Events  */

function onOpenInsecureButtonClick() {
  RPMSendAsyncMessage("openInsecure");
}

function onReturnButtonClick() {
  RPMSendAsyncMessage("goBack");
}

/*  Utils */

function addAutofocus(selector, position = "afterbegin") {
  if (window.top != window) {
    return;
  }
  var button = document.querySelector(selector);
  var parent = button.parentNode;
  button.remove();
  button.setAttribute("autofocus", "true");
  parent.insertAdjacentElement(position, button);
}

/* Initialize Page */

initPage();
// Dispatch this event so tests can detect that we finished loading the error page.
// We're using the same event name as neterror because BrowserTestUtils.jsm relies on that.
let event = new CustomEvent("AboutNetErrorLoad", { bubbles: true });
document.dispatchEvent(event);
