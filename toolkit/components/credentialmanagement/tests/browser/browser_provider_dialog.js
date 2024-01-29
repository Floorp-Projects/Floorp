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

// Test that a single provider shows up in a dialog and is chosen when "continue" is clicked
add_task(async function test_single_provider_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show one IDP
  let prompt = IdentityCredentialPromptService.showProviderPrompt(
    tab.linkedBrowser.browsingContext,
    [
      {
        configURL: "https://idp.example/",
        clientId: "123",
      },
    ],
    [
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
    ]
  );

  // Make sure a popup shows up.
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Make sure there is only one option
  let inputs = document
    .getElementById("identity-credential-provider")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 1, "One IDP");

  // Make sure the IDP Site is in the label
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(label.textContent.includes("idp.example"), "IDP site in label");

  // Validate the title of the popup
  let title = document.querySelector(
    'description[popupid="identity-credential"]'
  );
  ok(
    title.textContent.includes("Sign in with a login provider"),
    "Popup title correct"
  );

  // Click "Continue"
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure the prompt promise resolves
  let value = await prompt;
  is(value, 0, "Selected the only IDP");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Test that a single provider shows up in a dialog and is not chosen when "cancel" is clicked
add_task(async function test_single_provider_deny() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show one IDP
  let prompt = IdentityCredentialPromptService.showProviderPrompt(
    tab.linkedBrowser.browsingContext,
    [
      {
        configURL: "https://idp.example/",
        clientId: "123",
      },
    ],
    [
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
    ]
  );

  // Make sure a popup shows up.
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Click cancel
  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;
  document
    .getElementsByClassName("popup-notification-secondary-button")[0]
    .click();

  try {
    await prompt;
    ok(false, "Prompt should not resolve when denied.");
  } catch (e) {
    ok(true, "Prompt should reject when denied.");
  }

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Show multiple IDPs and select the first one
add_task(async function test_multiple_provider_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show two providers, in order. (we don't need the metadata because we aren't testing branding)
  let prompt = IdentityCredentialPromptService.showProviderPrompt(
    tab.linkedBrowser.browsingContext,
    [
      {
        configURL: "https://idp1.example/",
        clientId: "123",
      },
      {
        configURL: "https://idp2.example/",
        clientId: "123",
      },
    ],
    [
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
    ]
  );

  // Make sure the popup shows up
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  let inputs = document
    .getElementById("identity-credential-provider")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 2, "Two IDPs visible");

  let label1 = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(
    label1.textContent.includes("idp1.example"),
    "First IDP label includes its site"
  );
  let label2 = inputs[1].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(
    label2.textContent.includes("idp2.example"),
    "Second IDP label includes its site"
  );

  // Click continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  let value = await prompt;
  is(value, 0, "The default is the first option in the list");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Show multiple IDPs and select the second one
add_task(async function test_multiple_provider_choose_second() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show two providers, in order. (we don't need the metadata because we aren't testing branding)
  let prompt = IdentityCredentialPromptService.showProviderPrompt(
    tab.linkedBrowser.browsingContext,
    [
      {
        configURL: "https://idp1.example/",
        clientId: "123",
      },
      {
        configURL: "https://idp2.example/",
        clientId: "123",
      },
    ],
    [
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
    ]
  );

  // Wait for the popup
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  let inputs = document
    .getElementById("identity-credential-provider")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 2, "Two IDPs visible");

  let label1 = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(
    label1.textContent.includes("idp1.example"),
    "First IDP label includes its site"
  );
  let label2 = inputs[1].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(
    label2.textContent.includes("idp2.example"),
    "Second IDP label includes its site"
  );

  // Click the second list item
  inputs[1].click();

  // Click continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure the caller gets the second IDP
  let value = await prompt;
  is(value, 1, "Choosing a different option makes a change");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Validate that the branding information is rendered correctly
add_task(async function test_multiple_provider_show_branding() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show the prompt, but include an icon for the second IDP
  let prompt = IdentityCredentialPromptService.showProviderPrompt(
    tab.linkedBrowser.browsingContext,
    [
      {
        configURL: "https://idp1.example/",
        clientId: "123",
      },
      {
        configURL: "https://idp2.example/",
        clientId: "123",
      },
    ],
    [
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
      },
      {
        accounts_endpoint: "",
        client_metadata_endpoint: "",
        id_assertion_endpoint: "",
        branding: {
          icons: [
            {
              url: "https://example.net/browser/toolkit/components/credentialmanagement/tests/browser/custom.svg",
            },
          ],
          name: "demo ip",
        },
      },
    ]
  );

  // Wait for that popup
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Validate the icons
  let icons = document
    .getElementById("identity-credential-provider")
    .getElementsByClassName("identity-credential-list-item-icon");
  is(icons.length, 2, "Two icons in the popup");
  Assert.notEqual(icons[0].src, icons[1].src, "Icons are different");
  ok(
    icons[1].src.startsWith(
      "data:image/svg+xml;base64,PCEtLSBUaGlzIFNvdXJjZSBDb2RlIEZvcm0gaXMgc3ViamVjdCB0byB0aGUgdGVybXMgb2YgdGhlIE1vemlsbGEgUHVibGljCiAgIC0gTGljZW5zZSwgdi4gMi4wLiBJZiBhIGNvcHkgb2YgdGhlIE1QTCB3YXMgbm90IGRpc3RyaWJ1dGVkIHdpdGggdGhpcwogICAtIGZpbGUsIFlvdSBjYW4gb2J0YWluIG9uZSBhdCBodHRwOi8vbW96aWxsYS5vcmcvTVBMLzIuMC8uIC0tPgo8c3ZnIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDE2IDE2IiB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIGZpbGw9ImNvbnRleHQtZmlsbCIgZmlsbC1vcGFjaXR5PSJjb250ZXh0LWZpbGwtb3BhY2l0eSI+CiAgPHBhdGggZD0iTS42MjUgMTNhLjYyNS42MjUgMCAwIDEgMC0xLjI1bDMuMjUgMEE0Ljg4IDQuODggMCAwIDAgOC43NSA2Ljg3NWwwLS4yNWEuNjI1LjYyNSAwIDAgMSAxLjI1IDBsMCAuMjVBNi4xMzIgNi4xMzIgMCAwIDEgMy44NzUgMTNsLTMuMjUgMHoiLz"
    ),
    "The second icon matches the custom.svg"
  );

  let inputs = document
    .getElementById("identity-credential-provider")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 2, "One IDP");
  let label = inputs[1].getElementsByClassName(
    "identity-credential-list-item-label-primary"
  )[0];
  ok(label.textContent.includes("demo ip"), "should show business short time");

  // Click continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure the caller gets the first provider still
  let value = await prompt;
  is(value, 0, "The default is the first option in the list");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});
