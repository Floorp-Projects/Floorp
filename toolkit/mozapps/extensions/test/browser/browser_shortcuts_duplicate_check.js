"use strict";

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
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

  let warningBars = doc.querySelectorAll("message-bar");
  // Ensure warning messages are shown for each duplicate shorctut.
  is(
    warningBars.length,
    Object.keys(duplicateCommands).length,
    "There is a warning message bar for each duplicate shortcut"
  );

  // Ensure warning messages are correct with correct shortcuts.
  let count = 1;
  for (let warning of warningBars) {
    let warningMsg = warning.querySelector("span");
    let l10nAttrs = doc.l10n.getAttributes(warningMsg);
    is(
      l10nAttrs.id,
      "shortcuts-duplicate-warning-message",
      "Warning message is shown"
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
    let warningMsg = warning.querySelector("span");
    let l10nAttrs = doc.l10n.getAttributes(warningMsg);
    is(
      l10nAttrs.id,
      "shortcuts-duplicate-warning-message",
      "Warning message is shown"
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
