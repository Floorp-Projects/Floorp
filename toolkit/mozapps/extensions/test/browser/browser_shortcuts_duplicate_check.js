"use strict";

const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

async function loadShortcutsView() {
  let win = await loadInitialView("extension");

  // There should be a manage shortcuts link.
  let shortcutsLink = win.document.querySelector('[action="manage-shortcuts"]');

  // Open the shortcuts view.
  let loaded = waitForViewLoad(win);
  shortcutsLink.click();
  await loaded;

  return win;
}

add_task(async function testDuplicateShortcutsWarnings() {
  let duplicateCommands = {
    commandOne: {
      suggested_key: { default: "Shift+Alt+1" },
    },
    commandTwo: {
      description: "Command Two!",
      suggested_key: { default: "Shift+Alt+2" },
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: duplicateCommands,
      name: "Extension 1",
    },
    background() {
      browser.test.sendMessage("ready");
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        ...duplicateCommands,
        commandThree: {
          description: "Command Three!",
          suggested_key: { default: "Shift+Alt+3" },
        },
      },
      name: "Extension 2",
    },
    background() {
      browser.test.sendMessage("ready");
    },
    useAddonManager: "temporary",
  });

  await extension2.startup();
  await extension2.awaitMessage("ready");

  let win = await loadShortcutsView();
  let doc = win.document;

  let warningBars = doc.querySelectorAll("moz-message-bar");
  // Ensure warning messages are shown for each duplicate shorctut.
  is(
    warningBars.length,
    Object.keys(duplicateCommands).length,
    "There is a warning message bar for each duplicate shortcut"
  );

  // Ensure warning messages are correct with correct shortcuts.
  let count = 1;
  for (let warning of warningBars) {
    let l10nAttrs = doc.l10n.getAttributes(warning);
    await TestUtils.waitForCondition(() => warning.message !== "");
    ok(warning.message !== "", "Warning message attribute is set");
    is(
      l10nAttrs.id,
      "shortcuts-duplicate-warning-message2",
      "Warning message l10nId is correct"
    );
    Assert.deepEqual(
      l10nAttrs.args,
      { shortcut: `Shift+Alt+${count}` },
      "Warning message shortcut is correct"
    );
    count++;
  }

  ["Shift+Alt+1", "Shift+Alt+2"].forEach((shortcut, index) => {
    // Ensure warning messages are correct with correct shortcuts.
    let warning = warningBars[index];
    let l10nAttrs = doc.l10n.getAttributes(warning);
    ok(warning.message !== "", "Warning message attribute is set");
    is(
      l10nAttrs.id,
      "shortcuts-duplicate-warning-message2",
      "Warning message l10nId is correct"
    );
    Assert.deepEqual(
      l10nAttrs.args,
      { shortcut },
      "Warning message shortcut is correct"
    );

    // Check if all inputs have warning style.
    let inputs = doc.querySelectorAll(`input[shortcut="${shortcut}"]`);
    for (let input of inputs) {
      // Check if warning error message is shown on focus.
      input.focus();
      let error = doc.querySelector(".error-message");
      let label = error.querySelector(".error-message-label");
      is(error.style.visibility, "visible", "The error element is shown");
      is(
        error.getAttribute("type"),
        "warning",
        "Duplicate shortcut has warning class"
      );
      is(
        label.dataset.l10nId,
        "shortcuts-duplicate",
        "Correct error message is shown"
      );

      // On keypress events wrning class should be removed.
      EventUtils.synthesizeKey("A");
      ok(
        !error.classList.contains("warning"),
        "Error element should not have warning class"
      );

      input.blur();
      is(
        error.style.visibility,
        "hidden",
        "The error element is hidden on blur"
      );
    }
  });

  await closeView(win);
  await extension.unload();
  await extension2.unload();
});

add_task(async function testDuplicateShortcutOnMacOSCtrlKey() {
  if (AppConstants.platform !== "macosx") {
    ok(
      true,
      `Skipping macos specific test on platform ${AppConstants.platform}`
    );
    return;
  }

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Extension 1",
      browser_specific_settings: {
        gecko: { id: "extension1@mochi.test" },
      },
      commands: {
        commandOne: {
          // Cover expected mac normalized shortcut on default shortcut.
          suggested_key: { default: "Ctrl+Shift+1" },
        },
        commandTwo: {
          suggested_key: {
            default: "Alt+Shift+2",
            // Cover expected mac normalized shortcut on mac-specific shortcut.
            mac: "Ctrl+Shift+2",
          },
        },
      },
    },
  });

  const extension2 = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Extension 2",
      browser_specific_settings: {
        gecko: { id: "extension2@mochi.test" },
      },
      commands: {
        anotherCommand: {},
      },
    },
  });

  await extension.startup();
  await extension2.startup();

  const win = await loadShortcutsView();
  const doc = win.document;
  const errorEl = doc.querySelector("addon-shortcuts .error-message");
  const errorLabel = errorEl.querySelector(".error-message-label");

  ok(
    BrowserTestUtils.is_hidden(errorEl),
    "Expect shortcut error element to be initially hidden"
  );

  const getShortcutInput = commandName =>
    doc.querySelector(`input.shortcut-input[name="${commandName}"]`);

  const assertDuplicateShortcutWarning = async msg => {
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.is_visible(errorEl),
      `Wait for the shortcut-duplicate error to be visible on ${msg}`
    );
    Assert.deepEqual(
      document.l10n.getAttributes(errorLabel),
      {
        id: "shortcuts-exists",
        args: { addon: "Extension 1" },
      },
      `Got the expected warning message on duplicate shortcut on ${msg}`
    );
  };

  const clearWarning = async inputEl => {
    anotherCommandInput.blur();
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.is_hidden(errorEl),
      "Wait for the shortcut-duplicate error to be hidden"
    );
  };

  const anotherCommandInput = getShortcutInput("anotherCommand");
  anotherCommandInput.focus();
  EventUtils.synthesizeKey("1", { metaKey: true, shiftKey: true });

  await assertDuplicateShortcutWarning("shortcut conflict with commandOne");
  await clearWarning(anotherCommandInput);

  anotherCommandInput.focus();
  EventUtils.synthesizeKey("2", { metaKey: true, shiftKey: true });

  await assertDuplicateShortcutWarning("shortcut conflict with commandTwo");
  await clearWarning(anotherCommandInput);

  await closeView(win);
  await extension.unload();
  await extension2.unload();
});
