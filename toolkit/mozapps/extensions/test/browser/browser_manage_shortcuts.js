"use strict";

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

function extensionShortcutsReady(id) {
  let extension = WebExtensionPolicy.getByID(id).extension;
  return BrowserTestUtils.waitForCondition(() => {
    return extension.shortcuts.keysetsMap.has(window);
  }, "Wait for add-on keyset to be registered");
}

async function loadShortcutsView() {
  // Load the theme view initially so we can verify that the category is switched
  // to "extension" when the shortcuts view is loaded.
  let win = await loadInitialView("theme");
  let categoryUtils = new CategoryUtilities(win);

  is(
    categoryUtils.getSelectedViewId(),
    "addons://list/theme",
    "The theme category is selected"
  );

  let shortcutsLink = win.document.querySelector(
    '#page-options [action="manage-shortcuts"]'
  );
  ok(!shortcutsLink.hidden, "The shortcuts link is visible");

  let loaded = waitForViewLoad(win);
  shortcutsLink.click();
  await loaded;

  is(
    categoryUtils.getSelectedViewId(),
    "addons://list/extension",
    "The extension category is now selected"
  );

  return win;
}

add_task(async function testUpdatingCommands() {
  let commands = {
    commandZero: {},
    commandOne: {
      suggested_key: { default: "Shift+Alt+7" },
    },
    commandTwo: {
      description: "Command Two!",
      suggested_key: { default: "Alt+4" },
    },
    _execute_browser_action: {
      suggested_key: { default: "Shift+Alt+9" },
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands,
      browser_action: { default_popup: "popup.html" },
    },
    background() {
      browser.commands.onCommand.addListener(commandName => {
        browser.test.sendMessage("oncommand", commandName);
      });
      browser.test.sendMessage("ready");
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  await extensionShortcutsReady(extension.id);

  async function checkShortcut(name, key, modifiers) {
    EventUtils.synthesizeKey(key, modifiers);
    let message = await extension.awaitMessage("oncommand");
    is(
      message,
      name,
      `Expected onCommand listener to fire with the correct name: ${name}`
    );
  }

  // Load the about:addons shortcut view before verify that emitting
  // the key events does trigger the expected extension commands.
  // There is apparently a race (more likely to be triggered on an
  // optimized build) between:
  // - the new opened browser window to be ready to listen for the
  //   keyboard events that are expected to triggered one of the key
  //   in the extension keyset
  // - and the test calling EventUtils.syntesizeKey to test that
  //   the expected extension command listener is notified.
  //
  // Loading the shortcut view before calling checkShortcut seems to be
  // enough to consistently avoid that race condition.
  let win = await loadShortcutsView();

  // Check that the original shortcuts work.
  await checkShortcut("commandOne", "7", { shiftKey: true, altKey: true });
  await checkShortcut("commandTwo", "4", { altKey: true });

  let doc = win.document;

  let card = doc.querySelector(`.card[addon-id="${extension.id}"]`);
  ok(card, `There is a card for the extension`);

  let inputs = card.querySelectorAll(".shortcut-input");
  is(
    inputs.length,
    Object.keys(commands).length,
    "There is an input for each command"
  );

  let nameOrder = Array.from(inputs).map(input => input.getAttribute("name"));
  Assert.deepEqual(
    nameOrder,
    ["commandOne", "commandTwo", "_execute_browser_action", "commandZero"],
    "commandZero should be last since it is unset"
  );

  let count = 1;
  for (let input of inputs) {
    // Change the shortcut.
    input.focus();
    EventUtils.synthesizeKey("8", { shiftKey: true, altKey: true });
    count++;

    // Wait for the shortcut attribute to change.
    await BrowserTestUtils.waitForCondition(
      () => input.getAttribute("shortcut") == "Alt+Shift+8",
      "Wait for shortcut to update to Alt+Shift+8"
    );

    // Check that the change worked (but skip if browserAction).
    if (input.getAttribute("name") != "_execute_browser_action") {
      await checkShortcut(input.getAttribute("name"), "8", {
        shiftKey: true,
        altKey: true,
      });
    }

    // Change it again so it doesn't conflict with the next command.
    input.focus();
    EventUtils.synthesizeKey(count.toString(), {
      shiftKey: true,
      altKey: true,
    });
    await BrowserTestUtils.waitForCondition(
      () => input.getAttribute("shortcut") == `Alt+Shift+${count}`,
      `Wait for shortcut to update to Alt+Shift+${count}`
    );
  }

  // Check that errors can be shown.
  let input = inputs[0];
  let error = doc.querySelector(".error-message");
  let label = error.querySelector(".error-message-label");
  is(error.style.visibility, "hidden", "The error is initially hidden");

  // Try a shortcut with only shift for a modifier.
  input.focus();
  EventUtils.synthesizeKey("J", { shiftKey: true });
  let possibleErrors = ["shortcuts-modifier-mac", "shortcuts-modifier-other"];
  ok(possibleErrors.includes(label.dataset.l10nId), `The message is set`);
  is(error.style.visibility, "visible", "The error is shown");

  // Escape should clear the focus and hide the error.
  is(doc.activeElement, input, "The input is focused");
  EventUtils.synthesizeKey("Escape", {});
  ok(doc.activeElement != input, "The input is no longer focused");
  is(error.style.visibility, "hidden", "The error is hidden");

  // Check if assigning already assigned shortcut is prevented.
  input.focus();
  EventUtils.synthesizeKey("2", { shiftKey: true, altKey: true });
  is(label.dataset.l10nId, "shortcuts-exists", `The message is set`);
  is(error.style.visibility, "visible", "The error is shown");

  // Check the label uses the description first, and has a default for the special cases.
  function checkLabel(name, value) {
    let input = doc.querySelector(`.shortcut-input[name="${name}"]`);
    let label = input.previousElementSibling;
    if (label.dataset.l10nId) {
      is(label.dataset.l10nId, value, "The l10n-id is set");
    } else {
      is(label.textContent, value, "The textContent is set");
    }
  }
  checkLabel("commandOne", "commandOne");
  checkLabel("commandTwo", "Command Two!");
  checkLabel("_execute_browser_action", "shortcuts-browserAction2");

  await closeView(win);
  await extension.unload();
});

async function startExtensionWithCommands(numCommands) {
  let commands = {};

  for (let i = 0; i < numCommands; i++) {
    commands[`command-${i}`] = {};
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands,
    },
    background() {
      browser.test.sendMessage("ready");
    },
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  await extensionShortcutsReady(extension.id);

  return extension;
}

add_task(async function testExpanding() {
  const numCommands = 7;
  const visibleCommands = 5;

  let extension = await startExtensionWithCommands(numCommands);

  let win = await loadShortcutsView();
  let doc = win.document;

  let card = doc.querySelector(`.card[addon-id="${extension.id}"]`);
  ok(!card.hasAttribute("expanded"), "The card is not expanded");

  let shortcutRows = card.querySelectorAll(".shortcut-row");
  is(shortcutRows.length, numCommands, `There are ${numCommands} shortcuts`);

  function assertCollapsedVisibility() {
    for (let i = 0; i < shortcutRows.length; i++) {
      let row = shortcutRows[i];
      if (i < visibleCommands) {
        ok(
          getComputedStyle(row).display != "none",
          `The first ${visibleCommands} rows are visible`
        );
      } else {
        is(getComputedStyle(row).display, "none", "The other rows are hidden");
      }
    }
  }

  // Check the visibility of the rows.
  assertCollapsedVisibility();

  let expandButton = card.querySelector(".expand-button");
  ok(expandButton, "There is an expand button");
  let l10nAttrs = doc.l10n.getAttributes(expandButton);
  is(l10nAttrs.id, "shortcuts-card-expand-button", "The expand text is shown");
  is(
    l10nAttrs.args.numberToShow,
    numCommands - visibleCommands,
    "The number to be shown is set on the expand button"
  );

  // Expand the card.
  expandButton.click();

  is(card.getAttribute("expanded"), "true", "The card is now expanded");

  for (let row of shortcutRows) {
    ok(getComputedStyle(row).display != "none", "All the rows are visible");
  }

  // The collapse text is now shown.
  l10nAttrs = doc.l10n.getAttributes(expandButton);
  is(
    l10nAttrs.id,
    "shortcuts-card-collapse-button",
    "The colapse text is shown"
  );

  // Collapse the card.
  expandButton.click();

  ok(!card.hasAttribute("expanded"), "The card is now collapsed again");

  assertCollapsedVisibility({ collapsed: true });

  await closeView(win);
  await extension.unload();
});

add_task(async function testOneExtraCommandIsNotCollapsed() {
  const numCommands = 6;
  let extension = await startExtensionWithCommands(numCommands);

  let win = await loadShortcutsView();
  let doc = win.document;

  // The card is not expanded, since it doesn't collapse.
  let card = doc.querySelector(`.card[addon-id="${extension.id}"]`);
  ok(!card.hasAttribute("expanded"), "The card is not expanded");

  // Each shortcut has a row.
  let shortcutRows = card.querySelectorAll(".shortcut-row");
  is(shortcutRows.length, numCommands, `There are ${numCommands} shortcuts`);

  // There's no expand button, since it can't be expanded.
  let expandButton = card.querySelector(".expand-button");
  ok(!expandButton, "There is no expand button");

  // All of the rows are visible, to avoid a "Show 1 More" button.
  for (let row of shortcutRows) {
    ok(getComputedStyle(row).display != "none", "All the rows are visible");
  }

  await closeView(win);
  await extension.unload();
});
