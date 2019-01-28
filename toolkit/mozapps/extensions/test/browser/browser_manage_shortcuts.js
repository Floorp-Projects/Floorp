"use strict";

let gManagerWindow;
let gCategoryUtilities;

const {PromiseTestUtils} = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", {});
PromiseTestUtils.whitelistRejectionsGlobally(/Message manager disconnected/);

add_task(async function testUpdatingCommands() {
  let commands = {
    commandOne: {
      suggested_key: {default: "Shift+Alt+4"},
    },
    commandTwo: {
      description: "Command Two!",
      suggested_key: {default: "Alt+4"},
    },
    _execute_browser_action: {
      suggested_key: {default: "Shift+Alt+5"},
    },
  };
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands,
      browser_action: {default_popup: "popup.html"},
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

  gManagerWindow = await open_manager(null);
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  await gCategoryUtilities.openType("extension");

  async function checkShortcut(name, key, modifiers) {
    EventUtils.synthesizeKey(key, modifiers);
    let message = await extension.awaitMessage("oncommand");
    is(message, name, `Expected onCommand listener to fire with the correct name: ${name}`);
  }

  // Check that the original shortcuts work.
  await checkShortcut("commandOne", "4", {shiftKey: true, altKey: true});
  await checkShortcut("commandTwo", "4", {altKey: true});

  // Open the shortcuts view.
  let doc = gManagerWindow.document;
  let shortcutsLink = doc.getElementById("manage-shortcuts");
  shortcutsLink.click();
  await wait_for_view_load(gManagerWindow);

  doc = doc.getElementById("shortcuts-view").contentDocument;

  let card = doc.querySelector(`.card[addon-id="${extension.id}"]`);
  ok(card, `There is a card for the extension`);

  let inputs = card.querySelectorAll(".shortcut-input");
  is(inputs.length, Object.keys(commands).length, "There is an input for each command");

  for (let input of inputs) {
    // Change the shortcut.
    input.focus();
    EventUtils.synthesizeKey("7", {shiftKey: true, altKey: true});

    // Wait for the shortcut attribute to change.
    await BrowserTestUtils.waitForCondition(
      () => input.getAttribute("shortcut") == "Alt+Shift+7");

    // Check that the change worked (but skip if browserAction).
    if (input.getAttribute("name") != "_execute_browser_action") {
      await checkShortcut(input.getAttribute("name"), "7", {shiftKey: true, altKey: true});
    }

    // Change it again so it doesn't conflict with the next command.
    input.focus();
    EventUtils.synthesizeKey("9", {shiftKey: true, altKey: true});
    await BrowserTestUtils.waitForCondition(
      () => input.getAttribute("shortcut") == "Alt+Shift+9");
  }

  // Check that errors can be shown.
  let input = inputs[0];
  let error = doc.querySelector(".error-message");
  let label = error.querySelector(".error-message-label");
  is(error.style.visibility, "hidden", "The error is initially hidden");

  // Try a shortcut with only shift for a modifier.
  input.focus();
  EventUtils.synthesizeKey("J", {shiftKey: true});
  let possibleErrors = ["shortcuts-modifier-mac", "shortcuts-modifier-other"];
  ok(possibleErrors.includes(label.dataset.l10nId), `The message is set`);
  is(error.style.visibility, "visible", "The error is shown");

  // Escape should clear the focus and hide the error.
  is(doc.activeElement, input, "The input is focused");
  EventUtils.synthesizeKey("Escape", {});
  ok(doc.activeElement != input, "The input is no longer focused");
  is(error.style.visibility, "hidden", "The error is hidden");

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
  checkLabel("_execute_browser_action", "shortcuts-browserAction");

  await close_manager(gManagerWindow);
  await extension.unload();
});
