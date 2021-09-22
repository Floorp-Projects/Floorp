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
    "about-httpsonly-explanation-unavailable2",
    { websiteUrl: pageUrl.host }
  );

  const baseSupportURL = RPMGetFormatURLPref("app.support.baseURL");
  document
    .getElementById("learnMoreLink")
    .setAttribute("href", baseSupportURL + "https-only-prefs");

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

  const isTopLevel = window.top == window;
  const hasWWWPrefix = pageUrl.href.startsWith("https://www.");
  if (isTopLevel && !hasWWWPrefix) {
    // HTTPS-Only generally simply replaces http: with https:;
    // here we additionally try to add www and see if that allows to upgrade the connection if it is top level

    window.addEventListener("pingSecureWWWLinkSuccess", () => {
      activateSuggestionBox();
      displayWWWSuggestion(pageUrl.host);
    });

    // try to ping secure www link in the AboutHttpsOnlyErrorChild
    RPMTryPingSecureWWWLink();
  }
}

/*  Suggestion Box */

function activateSuggestionBox() {
  const suggestionBox = document.querySelector(".suggestion-box");
  suggestionBox.hidden = false;
}

function displayWWWSuggestion(aURL) {
  const suggestionBox = document.querySelector(".suggestion-box");
  const suggestionWWWText = document.createElement("p");
  const suggestionWWWButton = document.createElement("button");
  const suggestionButtonContainer = document.createElement("div");

  document.l10n.setAttributes(
    suggestionWWWText,
    "about-httpsonly-suggestion-box-www-text",
    { websiteUrl: aURL }
  );

  suggestionWWWButton.setAttribute("id", "openWWW");
  document.l10n.setAttributes(
    suggestionWWWButton,
    "about-httpsonly-suggestion-box-www-button",
    { websiteUrl: aURL }
  );
  suggestionWWWButton.addEventListener("click", openSecureWWWButtonClick);

  suggestionButtonContainer.classList.add("button-container");

  suggestionBox.appendChild(suggestionWWWText);
  suggestionButtonContainer.appendChild(suggestionWWWButton);
  suggestionBox.appendChild(suggestionButtonContainer);
}

/*  Button Events  */

function openSecureWWWButtonClick() {
  RPMOpenSecureWWWLink();
}

function onOpenInsecureButtonClick() {
  RPMSendAsyncMessage("openInsecure", {
    inFrame: window.top != window,
  });
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
