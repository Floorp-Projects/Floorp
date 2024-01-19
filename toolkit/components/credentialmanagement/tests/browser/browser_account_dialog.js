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

// Test that a single account shows up in a dialog and is chosen when "continue" is clicked
add_task(async function test_single_acccount_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show the single account
  let prompt = IdentityCredentialPromptService.showAccountListPrompt(
    tab.linkedBrowser.browsingContext,
    {
      accounts: [
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account",
          email: "test@idp.example",
        },
      ],
    },
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    }
  );

  // Wait for the popup to appear
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Validate the popup contents
  let inputs = document
    .getElementById("identity-credential-account")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("Test Account"),
    "Label includes the account name"
  );
  ok(
    label.textContent.includes("test@idp.example"),
    "Label includes the account email"
  );

  let title = document.getElementById("identity-credential-header-text");
  ok(
    title.textContent.includes("idp.example"),
    "Popup title includes the IDP Site"
  );

  // Click Continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure that the prompt resolves with the index of the only argument element
  let value = await prompt;
  is(value, 0);

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Test that no account is chosen when "cancel" is clicked
add_task(async function test_single_acccount_deny() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show a prompt with one account
  let prompt = IdentityCredentialPromptService.showAccountListPrompt(
    tab.linkedBrowser.browsingContext,
    {
      accounts: [
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account",
          email: "test@idp.example",
        },
      ],
    },
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    }
  );

  // Wait for that popup
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Validate the popup contents
  let inputs = document
    .getElementById("identity-credential-account")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("Test Account"),
    "Label includes the account name"
  );
  ok(
    label.textContent.includes("test@idp.example"),
    "Label includes the account email"
  );

  // Click cancel
  document
    .getElementsByClassName("popup-notification-secondary-button")[0]
    .click();

  // Make sure we reject
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

// Show multiple accounts and select the first one
add_task(async function test_multiple_acccount_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show a prompt with multiple accounts
  let prompt = IdentityCredentialPromptService.showAccountListPrompt(
    tab.linkedBrowser.browsingContext,
    {
      accounts: [
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account",
          email: "test@idp.example",
        },
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account 2",
          email: "test2@idp.example",
        },
      ],
    },
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    }
  );

  // Wait for that popup to appear
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Validate the account list contents and ordering
  let inputs = document
    .getElementById("identity-credential-account")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 2, "Two accounts expected");
  let label0 = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label0.textContent.includes("Test Account"),
    "The first account name should be in the label"
  );
  ok(
    label0.textContent.includes("test@idp.example"),
    "The first account email should be in the label"
  );
  let label1 = inputs[1].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label1.textContent.includes("Test Account 2"),
    "The second account name should be in the label"
  );
  ok(
    label1.textContent.includes("test2@idp.example"),
    "The second account email should be in the label"
  );

  // Click continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Validate that the caller gets a resolving promise with index of the first element
  let value = await prompt;
  is(value, 0, "The first account is chosen by default");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Show multiple accounts and select the second one
add_task(async function test_multiple_acccount_choose_second() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show a prompt with multiple accounts
  let prompt = IdentityCredentialPromptService.showAccountListPrompt(
    tab.linkedBrowser.browsingContext,
    {
      accounts: [
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account",
          email: "test@idp.example",
        },
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account 2",
          email: "test2@idp.example",
        },
      ],
    },
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
    }
  );

  // Wait for that popup
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  // Validate the account list contents and ordering
  let inputs = document
    .getElementById("identity-credential-account")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 2, "Two accounts expected");
  let label0 = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label0.textContent.includes("Test Account"),
    "The first account name should be in the label"
  );
  ok(
    label0.textContent.includes("test@idp.example"),
    "The first account email should be in the label"
  );
  let label1 = inputs[1].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label1.textContent.includes("Test Account 2"),
    "The second account name should be in the label"
  );
  ok(
    label1.textContent.includes("test2@idp.example"),
    "The second account email should be in the label"
  );

  // Click the second account
  inputs[1].click();

  // Click continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure we selected the second account
  let value = await prompt;
  is(value, 1, "The prompt should resolve to 1, indicating the second account");

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});

// Test that account pictures are rendered for the user
add_task(async function test_multiple_acccount_show_picture() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Show a prompt with two accounts, but only the first has a custom picture
  let prompt = IdentityCredentialPromptService.showAccountListPrompt(
    tab.linkedBrowser.browsingContext,
    {
      accounts: [
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account",
          email: "test@idp.example",
          picture:
            "https://example.net/browser/toolkit/components/credentialmanagement/tests/browser/custom.svg",
        },
        {
          id: "00000000-0000-0000-0000-000000000000",
          name: "Test Account 2",
          email: "test2@idp.example",
        },
      ],
    },
    {
      configURL: "https://idp.example/",
      clientId: "123",
    },
    {
      accounts_endpoint: "",
      client_metadata_endpoint: "",
      id_assertion_endpoint: "",
      privacy_policy_url: "https://idp.example/privacy-policy.html",
      terms_of_service_url: "https://idp.example/terms-of-service.html",
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
    }
  );

  // Wait for that popup
  await popupShown;

  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  let document = tab.linkedBrowser.browsingContext.topChromeWindow.document;

  let icons = document
    .getElementById("identity-credential-account")
    .getElementsByClassName("identity-credential-list-item-icon");
  is(icons.length, 2, "Two accounts expected");
  ok(icons[0].src != icons[1].src, "The icons are different");
  ok(
    icons[0].src.startsWith(
      "data:image/svg+xml;base64,PCEtLSBUaGlzIFNvdXJjZSBDb2RlIEZvcm0gaXMgc3ViamVjdCB0byB0aGUgdGVybXMgb2YgdGhlIE1vemlsbGEgUHVibGljCiAgIC0gTGljZW5zZSwgdi4gMi4wLiBJZiBhIGNvcHkgb2YgdGhlIE1QTCB3YXMgbm90IGRpc3RyaWJ1dGVkIHdpdGggdGhpcwogICAtIGZpbGUsIFlvdSBjYW4gb2J0YWluIG9uZSBhdCBodHRwOi8vbW96aWxsYS5vcmcvTVBMLzIuMC8uIC0tPgo8c3ZnIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDE2IDE2IiB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIGZpbGw9ImNvbnRleHQtZmlsbCIgZmlsbC1vcGFjaXR5PSJjb250ZXh0LWZpbGwtb3BhY2l0eSI+CiAgPHBhdGggZD0iTS42MjUgMTNhLjYyNS42MjUgMCAwIDEgMC0xLjI1bDMuMjUgMEE0Ljg4IDQuODggMCAwIDAgOC43NSA2Ljg3NWwwLS4yNWEuNjI1LjYyNSAwIDAgMSAxLjI1IDBsMCAuMjVBNi4xMzIgNi4xMzIgMCAwIDEgMy44NzUgMTNsLTMuMjUgMHoiLz"
    ),
    "The first icon matches the custom.svg"
  );

  const headerIcon = document.getElementsByClassName(
    "identity-credential-header-icon"
  )[0];

  let title = document.getElementById("identity-credential-header-text");
  ok(
    title.textContent.includes("demo ip"),
    "Popup title appears as business short name"
  );

  ok(BrowserTestUtils.isVisible(headerIcon), "Header Icon is showing");
  ok(
    headerIcon.src.startsWith(
      "data:image/svg+xml;base64,PCEtLSBUaGlzIFNvdXJjZSBDb2RlIEZvcm0gaXMgc3ViamVjdCB0byB0aGUgdGVybXMgb2YgdGhlIE1vemlsbGEgUHVibGljCiAgIC0gTGljZW5zZSwgdi4gMi4wLiBJZiBhIGNvcHkgb2YgdGhlIE1QTCB3YXMgbm90IGRpc3RyaWJ1dGVkIHdpdGggdGhpcwogICAtIGZpbGUsIFlvdSBjYW4gb2J0YWluIG9uZSBhdCBodHRwOi8vbW96aWxsYS5vcmcvTVBMLzIuMC8uIC0tPgo8c3ZnIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDE2IDE2IiB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIGZpbGw9ImNvbnRleHQtZmlsbCIgZmlsbC1vcGFjaXR5PSJjb250ZXh0LWZpbGwtb3BhY2l0eSI+CiAgPHBhdGggZD0iTS42MjUgMTNhLjYyNS42MjUgMCAwIDEgMC0xLjI1bDMuMjUgMEE0Ljg4IDQuODggMCAwIDAgOC43NSA2Ljg3NWwwLS4yNWEuNjI1LjYyNSAwIDAgMSAxLjI1IDBsMCAuMjVBNi4xMzIgNi4xMzIgMCAwIDEgMy44NzUgMTNsLTMuMjUgMHoiLz"
    ),
    "The header icon matches the icon resource from manifest"
  );

  // Click Continue
  document
    .getElementsByClassName("popup-notification-primary-button")[0]
    .click();

  // Make sure that the prompt resolves with the index of the only argument element
  let value = await prompt;
  is(value, 0);

  await popupHiding;

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});
