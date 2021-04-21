/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gHandlerService = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

// Testing multiple protocol / origin combinations takes long on debug.
requestLongerTimeout(3);

const DIALOG_URL_APP_CHOOSER =
  "chrome://mozapps/content/handling/appChooser.xhtml";
const DIALOG_URL_PERMISSION =
  "chrome://mozapps/content/handling/permissionDialog.xhtml";

const PROTOCOL_HANDLER_OPEN_PERM_KEY = "open-protocol-handler";
const PERMISSION_KEY_DELIMITER = "^";

const TEST_PROTOS = ["foo", "bar"];

let testDir = getChromeDir(getResolvedURI(gTestPath));

const ORIGIN1 = "https://example.com";
const ORIGIN2 = "https://example.org";
const ORIGIN3 = Services.io.newFileURI(testDir).spec;
const PRINCIPAL1 = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
  ORIGIN1
);
const PRINCIPAL2 = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
  ORIGIN2
);
const PRINCIPAL3 = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
  ORIGIN3
);

let testExtension;

/**
 * Get the open protocol handler permission key for a given protocol scheme.
 * @param {string} aProtocolScheme - Scheme of protocol to construct permission
 * key with.
 */
function getSkipProtoDialogPermissionKey(aProtocolScheme) {
  return (
    PROTOCOL_HANDLER_OPEN_PERM_KEY + PERMISSION_KEY_DELIMITER + aProtocolScheme
  );
}

/**
 * Creates dummy web protocol handlers used for testing.
 */
function initTestHandlers() {
  TEST_PROTOS.forEach(scheme => {
    let webHandler = Cc[
      "@mozilla.org/uriloader/web-handler-app;1"
    ].createInstance(Ci.nsIWebHandlerApp);
    webHandler.name = scheme + "Handler";
    webHandler.uriTemplate = ORIGIN1 + "/?url=%s";

    let handlerInfo = HandlerServiceTestUtils.getBlankHandlerInfo(scheme);
    handlerInfo.possibleApplicationHandlers.appendElement(webHandler);
    handlerInfo.preferredApplicationHandler = webHandler;

    gHandlerService.store(handlerInfo);
  });
}

/**
 * Update whether the protocol handler dialog is shown for our test protocol +
 * handler.
 * @param {string} scheme - Scheme of the protocol to change the ask state for.
 * @param {boolean} ask - true => show dialog, false => skip dialog.
 */
function updateAlwaysAsk(scheme, ask) {
  let handlerInfo = HandlerServiceTestUtils.getHandlerInfo(scheme);
  handlerInfo.alwaysAskBeforeHandling = ask;
  gHandlerService.store(handlerInfo);
}

/**
 * Test whether the protocol handler dialog is set to show for our
 * test protocol + handler.
 * @param {string} scheme - Scheme of the protocol to test the ask state for.
 * @param {boolean} ask - true => show dialog, false => skip dialog.
 */
function testAlwaysAsk(scheme, ask) {
  is(
    HandlerServiceTestUtils.getHandlerInfo(scheme).alwaysAskBeforeHandling,
    ask,
    "Should have correct alwaysAsk state."
  );
}

/**
 * Open a test URL with the desired scheme.
 * By default the load is triggered by the content principal of the browser.
 * @param {MozBrowser} browser - Browser to load the test URL in.
 * @param {string} scheme - Scheme of the test URL.
 * @param {Object} [opts] - Options for the triggering principal.
 * @param {nsIPrincipal} [opts.triggeringPrincipal] - Principal to trigger the
 * load with. Defaults to the browsers content principal.
 * @param {boolean} [opts.useNullPrincipal] - If true, we will trigger the load
 * with a null principal.
 * @param {boolean} [opts.useExtensionPrincipal] - If true, we will trigger the
 * load with an extension.
 * @param {boolean} [opts.omitTriggeringPrincipal] - If true, we will directly
 * call the protocol handler dialogs without a principal.
 */
async function triggerOpenProto(
  browser,
  scheme,
  {
    triggeringPrincipal = browser.contentPrincipal,
    useNullPrincipal = false,
    useExtensionPrincipal = false,
    omitTriggeringPrincipal = false,
  } = {}
) {
  let uri = `${scheme}://test`;

  if (useNullPrincipal) {
    // Create and load iframe with data URI.
    // This will be a null principal.
    ContentTask.spawn(browser, { uri }, args => {
      let frame = content.document.createElement("iframe");
      frame.src = `data:text/html,<script>location.href="${args.uri}"</script>`;
      content.document.body.appendChild(frame);
    });
    return;
  }

  if (useExtensionPrincipal) {
    const EXTENSION_DATA = {
      manifest: {
        content_scripts: [
          {
            matches: [browser.currentURI.spec],
            js: ["navigate.js"],
          },
        ],
      },
      files: {
        "navigate.js": `window.location.href = "${uri}";`,
      },
    };

    testExtension = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
    await testExtension.startup();
    return;
  }

  if (omitTriggeringPrincipal) {
    // Directly call ContentDispatchChooser without a triggering principal
    let contentDispatchChooser = Cc[
      "@mozilla.org/content-dispatch-chooser;1"
    ].createInstance(Ci.nsIContentDispatchChooser);

    let handler = HandlerServiceTestUtils.getHandlerInfo(scheme);

    contentDispatchChooser.handleURI(
      handler,
      Services.io.newURI(uri),
      null,
      browser.browsingContext
    );
    return;
  }

  info("Loading uri: " + uri);
  browser.loadURI(uri, { triggeringPrincipal });
}

/**
 * Navigates to a test URL with the given protocol scheme and waits for the
 * result.
 * @param {MozBrowser} browser - Browser to navigate.
 * @param {string} scheme - Scheme of the test url. e.g. irc
 * @param {Object} [options] - Test options.
 * @param {Object} [options.permDialogOptions] - Test options for the permission
 * dialog. If defined, we expect this dialog to be shown.
 * @param {Object} [options.chooserDialogOptions] - Test options for the chooser
 * dialog. If defined, we expect this dialog to be shown.
 * @param {Object} [options.loadOptions] - Options for triggering the protocol
 * load which causes the dialog to show.
 * @param {nsIPrincipal} [options.triggeringPrincipal] - Principal to trigger
 * the load with. Defaults to the browsers content principal.
 * @returns {Promise} - A promise which resolves once the test is complete.
 */
async function testOpenProto(
  browser,
  scheme,
  { permDialogOptions, chooserDialogOptions, loadOptions } = {}
) {
  let permDialogOpenPromise;
  let chooserDialogOpenPromise;

  if (permDialogOptions) {
    info("Should see permission dialog");
    permDialogOpenPromise = waitForProtocolPermissionDialog(browser, true);
  }

  if (chooserDialogOptions) {
    info("Should see chooser dialog");
    chooserDialogOpenPromise = waitForProtocolAppChooserDialog(browser, true);
  }
  await triggerOpenProto(browser, scheme, loadOptions);
  let webHandlerLoadedPromise;

  let webHandlerShouldOpen =
    (!permDialogOptions && !chooserDialogOptions) ||
    ((permDialogOptions?.actionConfirm || permDialogOptions?.actionChangeApp) &&
      chooserDialogOptions?.actionConfirm);

  // Register web handler load listener if we expect to trigger it.
  if (webHandlerShouldOpen) {
    webHandlerLoadedPromise = waitForHandlerURL(browser, scheme);
  }

  if (permDialogOpenPromise) {
    let dialog = await permDialogOpenPromise;
    let dialogEl = getDialogElementFromSubDialog(dialog);
    let dialogType = getDialogType(dialog);

    let {
      hasCheckbox,
      hasChangeApp,
      chooserIsNext,
      actionCheckbox,
      actionConfirm,
      actionChangeApp,
    } = permDialogOptions;

    if (actionChangeApp) {
      actionConfirm = false;
    }

    await testCheckbox(dialogEl, dialogType, {
      hasCheckbox,
      actionCheckbox,
    });

    // Check the button label depending on whether we would show the chooser
    // dialog next or directly open the handler.
    let acceptBtnLabel = dialogEl.getButton("accept")?.label;
    if (chooserIsNext) {
      is(
        acceptBtnLabel,
        "Choose Application",
        "Accept button has choose app label"
      );
    } else {
      is(acceptBtnLabel, "Open Link", "Accept button has open link label");
    }

    let changeAppLink = dialogEl.ownerDocument.getElementById("change-app");
    if (typeof hasChangeApp == "boolean") {
      ok(changeAppLink, "Permission dialog should have changeApp link label");
      is(
        !changeAppLink.hidden,
        hasChangeApp,
        "Permission dialog change app link label"
      );
    }

    if (actionChangeApp) {
      let dialogClosedPromise = waitForProtocolPermissionDialog(browser, false);
      changeAppLink.click();
      await dialogClosedPromise;
    } else {
      await closeDialog(browser, dialog, actionConfirm, scheme);
    }
  }

  if (chooserDialogOpenPromise) {
    let dialog = await chooserDialogOpenPromise;
    let dialogEl = getDialogElementFromSubDialog(dialog);
    let dialogType = getDialogType(dialog);

    let { hasCheckbox, actionCheckbox, actionConfirm } = chooserDialogOptions;

    await testCheckbox(dialogEl, dialogType, {
      hasCheckbox,
      actionCheckbox,
    });

    await closeDialog(browser, dialog, actionConfirm, scheme);
  }

  if (webHandlerShouldOpen) {
    info("Waiting for web handler to open");
    await webHandlerLoadedPromise;
  } else {
    info("Web handler open canceled");
  }

  // Clean up test extension if needed.
  await testExtension?.unload();
}

/**
 * Inspects the checkbox state and interacts with it.
 * @param {dialog} dialogEl
 * @param {string} dialogType - String identifier of dialog type.
 * Either "permission" or "chooser".
 * @param {Object} options - Test Options.
 * @param {boolean} [options.hasCheckbox] - Whether the dialog is expected to
 * have a visible checkbox.
 * @param {boolean} [options.hasCheckboxState] - The check state of the checkbox
 * to test for. true = checked, false = unchecked.
 * @param {boolean} [options.actionCheckbox] - The state to set on the checkbox.
 * true = checked, false = unchecked.
 */
async function testCheckbox(
  dialogEl,
  dialogType,
  { hasCheckbox, hasCheckboxState = false, actionCheckbox }
) {
  let checkbox = dialogEl.ownerDocument.getElementById("remember");
  if (typeof hasCheckbox == "boolean") {
    is(
      checkbox && BrowserTestUtils.is_visible(checkbox),
      hasCheckbox,
      "Dialog checkbox has correct visibility."
    );
  }

  if (typeof hasCheckboxState == "boolean") {
    is(checkbox.checked, hasCheckboxState, "Dialog checkbox has correct state");
  }

  if (typeof actionCheckbox == "boolean") {
    checkbox.focus();
    await EventUtils.synthesizeKey("VK_SPACE", undefined, dialogEl.ownerWindow);
  }
}

/**
 * Get the dialog element which is a child of the SubDialogs browser frame.
 * @param {SubDialog} subDialog - Dialog to get the dialog element for.
 */
function getDialogElementFromSubDialog(subDialog) {
  let dialogEl = subDialog._frame.contentDocument.querySelector("dialog");
  ok(dialogEl, "SubDialog should have dialog element");
  return dialogEl;
}

/**
 * Wait for the test handler to be opened.
 * @param {MozBrowser} browser - The browser the load should occur in.
 * @param {string} scheme - Scheme which triggered the handler to open.
 */
function waitForHandlerURL(browser, scheme) {
  return BrowserTestUtils.browserLoaded(
    browser,
    false,
    url => url == `${ORIGIN1}/?url=${scheme}%3A%2F%2Ftest`
  );
}

/**
 * Test for open-protocol-handler permission.
 * @param {nsIPrincipal} principal - The principal to test the permission on.
 * @param {string} scheme - Scheme to generate permission key.
 * @param {boolean} hasPerm - Whether we expect the princial to set the
 * permission (true), or not (false).
 */
function testPermission(principal, scheme, hasPerm) {
  let permKey = getSkipProtoDialogPermissionKey(scheme);
  let result = Services.perms.testPermissionFromPrincipal(principal, permKey);
  let message = `${permKey} ${hasPerm ? "is" : "is not"} set for ${
    principal.origin
  }.`;
  is(result == Services.perms.ALLOW_ACTION, hasPerm, message);
}

/**
 * Get the checkbox element of the dialog used to remember the handler choice or
 * store the permission.
 * @param {SubDialog} dialog - Protocol handler dialog embedded in a SubDialog.
 * @param {string} dialogType - Type of the dialog which holds the checkbox.
 * @returns {HTMLInputElement} - Checkbox of the dialog.
 */
function getDialogCheckbox(dialog, dialogType) {
  let id;
  if (dialogType == "permission") {
    id = "remember-permission";
  } else {
    id = "remember";
  }
  return dialog._frame.contentDocument.getElementById(id);
}

function getDialogType(dialog) {
  let url = dialog._frame.currentURI.spec;

  if (url === DIALOG_URL_PERMISSION) {
    return "permission";
  }
  if (url === DIALOG_URL_APP_CHOOSER) {
    return "chooser";
  }
  throw new Error("Dialog with unexpected url");
}

/**
 * Exit a protocol handler SubDialog and wait for it to be fully closed.
 * @param {MozBrowser} browser - Browser element of the tab where the dialog is
 * shown.
 * @param {SubDialog} dialog - SubDialog object which holds the protocol handler
 * @param {boolean} confirm - Whether to confirm (true) or cancel (false) the
 * dialog.
 * @param {string} scheme - The scheme of the protocol the dialog is opened for.
 * dialog.
 */
async function closeDialog(browser, dialog, confirm, scheme) {
  let dialogClosedPromise = waitForSubDialog(browser, dialog._openedURL, false);
  let dialogEl = getDialogElementFromSubDialog(dialog);

  if (confirm) {
    if (getDialogType(dialog) == "chooser") {
      // Select our test protocol handler
      let listItem = dialogEl.ownerDocument.querySelector(
        `richlistitem[name="${scheme}Handler"]`
      );
      listItem.click();
    }

    dialogEl.setAttribute("buttondisabledaccept", false);
    dialogEl.acceptDialog();
  } else {
    dialogEl.cancelDialog();
  }

  return dialogClosedPromise;
}

registerCleanupFunction(function() {
  // Clean up test handlers
  TEST_PROTOS.forEach(scheme => {
    let handlerInfo = HandlerServiceTestUtils.getHandlerInfo(scheme);
    gHandlerService.remove(handlerInfo);
  });

  // Clear permissions
  Services.perms.removeAll();
});

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.external_protocol_requires_permission", true]],
  });
  initTestHandlers();
});

/**
 * Tests that when "remember" is unchecked, we only allow the protocol to be
 * opened once and don't store any permission.
 */
add_task(async function test_permission_allow_once() {
  for (let scheme of TEST_PROTOS) {
    await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
      await testOpenProto(browser, scheme, {
        permDialogOptions: {
          hasCheckbox: true,
          hasChangeApp: false,
          chooserIsNext: true,
          actionConfirm: true,
        },
        chooserDialogOptions: { hasCheckbox: true, actionConfirm: true },
      });
    });

    // No permission should be set
    testPermission(PRINCIPAL1, scheme, false);
    testPermission(PRINCIPAL2, scheme, false);

    // No preferred app should be set
    testAlwaysAsk(scheme, true);

    // If we open again we should see the permission dialog
    await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
      await testOpenProto(browser, scheme, {
        permDialogOptions: {
          hasCheckbox: true,
          hasChangeApp: false,
          chooserIsNext: true,
          actionConfirm: false,
        },
      });
    });
  }
});

/**
 * Tests that when checking the "remember" checkbox, the protocol permission
 * is set correctly and allows the caller to skip the permission dialog in
 * subsequent calls.
 */
add_task(async function test_permission_allow_persist() {
  for (let [origin, principal] of [
    [ORIGIN1, PRINCIPAL1],
    [ORIGIN3, PRINCIPAL3],
  ]) {
    for (let scheme of TEST_PROTOS) {
      info("Testing with origin " + origin);
      info("testing with principal of origin " + principal.origin);
      info("testing with protocol " + scheme);

      // Set a permission for an unrelated protocol.
      // We should still see the permission dialog.
      Services.perms.addFromPrincipal(
        principal,
        getSkipProtoDialogPermissionKey("foobar"),
        Services.perms.ALLOW_ACTION
      );

      await BrowserTestUtils.withNewTab(origin, async browser => {
        await testOpenProto(browser, scheme, {
          permDialogOptions: {
            hasCheckbox: true,
            hasChangeApp: false,
            chooserIsNext: true,
            actionCheckbox: true,
            actionConfirm: true,
          },
          chooserDialogOptions: { hasCheckbox: true, actionConfirm: true },
        });
      });

      // Permission should be set
      testPermission(principal, scheme, true);
      testPermission(PRINCIPAL2, scheme, false);

      // No preferred app should be set
      testAlwaysAsk(scheme, true);

      // If we open again with the origin where we granted permission, we should
      // directly get the chooser dialog.
      await BrowserTestUtils.withNewTab(origin, async browser => {
        await testOpenProto(browser, scheme, {
          chooserDialogOptions: {
            hasCheckbox: true,
            actionConfirm: false,
          },
        });
      });

      // If we open with the other origin, we should see the permission dialog
      await BrowserTestUtils.withNewTab(ORIGIN2, async browser => {
        await testOpenProto(browser, scheme, {
          permDialogOptions: {
            hasCheckbox: true,
            hasChangeApp: false,
            chooserIsNext: true,
            actionConfirm: false,
          },
        });
      });

      // Cleanup permissions
      Services.perms.removeAll();
    }
  }
});

/**
 * Tests that if a preferred protocol handler is set, the permission dialog
 * shows the application name and a link which leads to the app chooser.
 */
add_task(async function test_permission_application_set() {
  let scheme = TEST_PROTOS[0];
  updateAlwaysAsk(scheme, false);
  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      permDialogOptions: {
        hasCheckbox: true,
        hasChangeApp: true,
        chooserIsNext: false,
        actionChangeApp: true,
      },
      chooserDialogOptions: { hasCheckbox: true, actionConfirm: true },
    });
  });

  // Cleanup
  updateAlwaysAsk(scheme, true);
});

/**
 * Tests that we correctly handle system principals. They should always
 * skip the permission dialog.
 */
add_task(async function test_permission_system_principal() {
  let scheme = TEST_PROTOS[0];
  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      chooserDialogOptions: { hasCheckbox: true, actionConfirm: false },
      loadOptions: {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      },
    });
  });
});

/**
 * Tests that we don't show the permission dialog if the permission is disabled
 * by pref.
 */
add_task(async function test_permission_disabled() {
  let scheme = TEST_PROTOS[0];

  await SpecialPowers.pushPrefEnv({
    set: [["security.external_protocol_requires_permission", false]],
  });

  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      chooserDialogOptions: { hasCheckbox: true, actionConfirm: true },
    });
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that we directly open the handler if permission and handler are set.
 */
add_task(async function test_app_and_permission_set() {
  let scheme = TEST_PROTOS[1];

  updateAlwaysAsk(scheme, false);
  Services.perms.addFromPrincipal(
    PRINCIPAL2,
    getSkipProtoDialogPermissionKey(scheme),
    Services.perms.ALLOW_ACTION
  );

  await BrowserTestUtils.withNewTab(ORIGIN2, async browser => {
    await testOpenProto(browser, scheme);
  });

  // Cleanup
  Services.perms.removeAll();
  updateAlwaysAsk(scheme, true);
});

/**
 * Tests that the alwaysAsk state is not updated if the user cancels the dialog
 */
add_task(async function test_change_app_checkbox_cancel() {
  let scheme = TEST_PROTOS[0];

  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      permDialogOptions: {
        hasCheckbox: true,
        chooserIsNext: true,
        hasChangeApp: false,
        actionConfirm: true,
      },
      chooserDialogOptions: {
        hasCheckbox: true,
        actionCheckbox: true, // Activate checkbox
        actionConfirm: false, // Cancel dialog
      },
    });
  });

  // Should not have applied value from checkbox
  testAlwaysAsk(scheme, true);
});

/**
 * Tests that the external protocol dialogs behave correctly when a null
 * principal is passed.
 */
add_task(async function test_null_principal() {
  let scheme = TEST_PROTOS[0];

  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      loadOptions: {
        useNullPrincipal: true,
      },
      permDialogOptions: {
        hasCheckbox: false,
        chooserIsNext: true,
        hasChangeApp: false,
        actionConfirm: true,
      },
      chooserDialogOptions: {
        hasCheckbox: true,
        actionConfirm: false, // Cancel dialog
      },
    });
  });
});

/**
 * Tests that the external protocol dialogs behave correctly when no principal
 * is passed.
 */
add_task(async function test_no_principal() {
  let scheme = TEST_PROTOS[1];

  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      loadOptions: {
        omitTriggeringPrincipal: true,
      },
      permDialogOptions: {
        hasCheckbox: false,
        chooserIsNext: true,
        hasChangeApp: false,
        actionConfirm: true,
      },
      chooserDialogOptions: {
        hasCheckbox: true,
        actionConfirm: false, // Cancel dialog
      },
    });
  });
});

/**
 * Tests that we skip the permission dialog for extension callers.
 */
add_task(async function test_extension_principal() {
  let scheme = TEST_PROTOS[0];
  await BrowserTestUtils.withNewTab(ORIGIN1, async browser => {
    await testOpenProto(browser, scheme, {
      loadOptions: {
        useExtensionPrincipal: true,
      },
      chooserDialogOptions: {
        hasCheckbox: true,
        actionConfirm: false, // Cancel dialog
      },
    });
  });
});
