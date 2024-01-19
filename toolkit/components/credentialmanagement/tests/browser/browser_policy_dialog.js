/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdentityCredentialPromptService",
  "@mozilla.org/browser/identitycredentialpromptservice;1",
  "nsIIdentityCredentialPromptService"
);

const TEST_URL = "https://example.com/";

// Test that a policy dialog does not appear when no policies are given
add_task(async function test_policy_dialog_empty() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let prompt = IdentityCredentialPromptService.showPolicyPrompt(
    tab.linkedBrowser.browsingContext,
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    },
    {} // No policies!
  );

  // Make sure we resolve with true without interaction
  let value = await prompt;
  is(value, true, "Automatically accept the missing policies");

  // Close tab
  await BrowserTestUtils.removeTab(tab);
});

// Make sure that a policy dialog shows up when we have policies to show.
// Also test the accept path.
add_task(async function test_policy_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show a prompt- the operative argument is the last one
  let prompt = IdentityCredentialPromptService.showPolicyPrompt(
    tab.linkedBrowser.browsingContext,
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
      branding: {
        background_color: "0x6200ee",
        color: "0xffffff",
        icons: [
          {
            size: 256,
            url: "https://example.net/browser/toolkit/components/credentialmanagement/tests/browser/custom.svg",
          },
        ],
        name: "demo ip",
      },
    },
    {
      privacy_policy_url: "https://idp.example/privacy-policy.html",
      terms_of_service_url: "https://idp.example/terms-of-service.html",
    }
  );

  // Make sure the popup shows up
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the contents of the popup
  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  let description = document.getElementById(
    "identity-credential-policy-explanation"
  );

  ok(
    description.textContent.includes("idp.example"),
    "IDP domain in the policy prompt text"
  );
  ok(
    description.textContent.includes("example.com"),
    "RP domain in the policy prompt text"
  );
  ok(
    description.textContent.includes("Privacy Policy"),
    "Link to the privacy policy in the policy prompt text"
  );
  ok(
    description.textContent.includes("Terms of Service"),
    "Link to the ToS in the policy prompt text"
  );

  let title = document.getElementById("identity-credential-header-text");
  ok(
    title.textContent.includes("demo ip"),
    "IDP domain in the policy prompt header as business short name"
  );

  const headerIcon = document.getElementsByClassName(
    "identity-credential-header-icon"
  )[0];

  ok(BrowserTestUtils.isVisible(headerIcon), "Header Icon is showing");
  ok(
    headerIcon.src.startsWith(
      "data:image/svg+xml;base64,PCEtLSBUaGlzIFNvdXJjZSBDb2RlIEZvcm0gaXMgc3ViamVjdCB0byB0aGUgdGVybXMgb2YgdGhlIE1vemlsbGEgUHVibGljCiAgIC0gTGljZW5zZSwgdi4gMi4wLiBJZiBhIGNvcHkgb2YgdGhlIE1QTCB3YXMgbm90IGRpc3RyaWJ1dGVkIHdpdGggdGhpcwogICAtIGZpbGUsIFlvdSBjYW4gb2J0YWluIG9uZSBhdCBodHRwOi8vbW96aWxsYS5vcmcvTVBMLzIuMC8uIC0tPgo8c3ZnIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDE2IDE2IiB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIGZpbGw9ImNvbnRleHQtZmlsbCIgZmlsbC1vcGFjaXR5PSJjb250ZXh0LWZpbGwtb3BhY2l0eSI+CiAgPHBhdGggZD0iTS42MjUgMTNhLjYyNS42MjUgMCAwIDEgMC0xLjI1bDMuMjUgMEE0Ljg4IDQuODggMCAwIDAgOC43NSA2Ljg3NWwwLS4yNWEuNjI1LjYyNSAwIDAgMSAxLjI1IDBsMCAuMjVBNi4xMzIgNi4xMzIgMCAwIDEgMy44NzUgMTNsLTMuMjUgMHoiLz"
    ),
    "The header icon matches the icon resource from manifest"
  );

  // Accept the policies
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure the call to the propmt resolves with true
  let value = await prompt;
  is(value, true, "User clicking accept resolves with true");

  // Wait for the prompt to go away
  await popupHiding;

  // Close tab
  await BrowserTestUtils.removeTab(tab);
});

// Test that rejecting the policies works
add_task(async function test_policy_reject() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show the same prompt with policies
  let prompt = IdentityCredentialPromptService.showPolicyPrompt(
    tab.linkedBrowser.browsingContext,
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    },
    {
      privacy_policy_url: "https://idp.example/privacy-policy.html",
      terms_of_service_url: "https://idp.example/terms-of-service.html",
    }
  );

  // Wait for the prompt to show up
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  let title = document.getElementById("identity-credential-header-text");
  ok(
    title.textContent.includes("idp.example"),
    "IDP domain in the policy prompt header as domain"
  );

  // Click reject.
  document
    .getElementsByClassName("popup-notification-secondary-button")[0]
    .click();

  // Make sure the prompt call accepts with an indication of the user's reject choice.
  let value = await prompt;
  is(value, false, "User clicking reject causes the promise to resolve(false)");

  // Wait for the popup to go away.
  await popupHiding;

  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});
