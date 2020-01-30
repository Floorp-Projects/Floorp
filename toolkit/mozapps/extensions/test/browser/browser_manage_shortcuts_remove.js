"use strict";

async function loadShortcutsView() {
  let managerWin = await open_manager(null);
  managerWin.gViewController.loadView("addons://shortcuts/shortcuts");
  await wait_for_view_load(managerWin);
  return managerWin.document.getElementById("html-view-browser")
    .contentDocument;
}

async function closeShortcutsView(doc) {
  let managerWin = doc.defaultView.parent;
  await close_manager(managerWin);
}

function getShortcutCard(doc, extension) {
  return doc.querySelector(`.shortcut[addon-id="${extension.id}"]`);
}

function getShortcutByName(doc, extension, name) {
  let card = getShortcutCard(doc, extension);
  return card && card.querySelector(`.shortcut-input[name="${name}"]`);
}

async function waitForShortcutSet(input, expected) {
  let doc = input.ownerDocument;
  await BrowserTestUtils.waitForCondition(
    () => input.getAttribute("shortcut") == expected,
    `Shortcut should be set to ${JSON.stringify(expected)}`
  );
  ok(doc.activeElement != input, "The input is no longer focused");
  checkHasRemoveButton(input, expected !== "");
}

function removeButtonForInput(input) {
  let removeButton = input.parentNode.querySelector(".shortcut-remove-button");
  ok(removeButton, "has remove button");
  return removeButton;
}

function checkHasRemoveButton(input, expected) {
  let removeButton = removeButtonForInput(input);
  let visibility = input.ownerGlobal.getComputedStyle(removeButton).visibility;
  if (expected) {
    is(visibility, "visible", "Remove button should be visible");
  } else {
    is(visibility, "hidden", "Remove button should be hidden");
  }
}

add_task(async function test_remove_shortcut() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        commandEmpty: {},
        commandOne: {
          suggested_key: { default: "Shift+Alt+1" },
        },
        commandTwo: {
          suggested_key: { default: "Shift+Alt+2" },
        },
      },
    },
    background() {
      browser.commands.onCommand.addListener(commandName => {
        browser.test.sendMessage("oncommand", commandName);
      });
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  let doc = await loadShortcutsView();

  let input = getShortcutByName(doc, extension, "commandOne");

  checkHasRemoveButton(input, true);

  // First: Verify that Shift-Del is not valid, but doesn't do anything.
  input.focus();
  EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
  let errorElem = doc.querySelector(".error-message");
  is(errorElem.style.visibility, "visible", "Expected error message");
  let errorId = doc.l10n.getAttributes(
    errorElem.querySelector(".error-message-label")
  ).id;
  if (AppConstants.platform == "macosx") {
    is(errorId, "shortcuts-modifier-mac", "Shift-Del is not a valid shortcut");
  } else {
    is(errorId, "shortcuts-modifier-other", "Shift-Del isn't a valid shortcut");
  }
  checkHasRemoveButton(input, true);

  // Now, verify that the original shortcut still works.
  EventUtils.synthesizeKey("KEY_Escape");
  ok(doc.activeElement != input, "The input is no longer focused");
  is(errorElem.style.visibility, "hidden", "The error is hidden");

  EventUtils.synthesizeKey("1", { altKey: true, shiftKey: true });
  await extension.awaitMessage("oncommand");

  // Alt-Shift-Del is a valid shortcut.
  input.focus();
  EventUtils.synthesizeKey("KEY_Delete", { altKey: true, shiftKey: true });
  await waitForShortcutSet(input, "Alt+Shift+Delete");
  EventUtils.synthesizeKey("KEY_Delete", { altKey: true, shiftKey: true });
  await extension.awaitMessage("oncommand");

  // Del without modifiers should clear the shortcut.
  input.focus();
  EventUtils.synthesizeKey("KEY_Delete");
  await waitForShortcutSet(input, "");
  // Trigger the shortcuts that were originally associated with commandOne,
  // and then trigger commandTwo. The extension should only see commandTwo.
  EventUtils.synthesizeKey("1", { altKey: true, shiftKey: true });
  EventUtils.synthesizeKey("KEY_Delete", { altKey: true, shiftKey: true });
  EventUtils.synthesizeKey("2", { altKey: true, shiftKey: true });
  is(
    await extension.awaitMessage("oncommand"),
    "commandTwo",
    "commandOne should be disabled, commandTwo should still be enabled"
  );

  // Set a shortcut where the default was not set.
  let inputEmpty = getShortcutByName(doc, extension, "commandEmpty");
  is(inputEmpty.getAttribute("shortcut"), "", "Empty shortcut by default");
  checkHasRemoveButton(input, false);
  inputEmpty.focus();
  EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  await waitForShortcutSet(inputEmpty, "Alt+Shift+3");
  EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  await extension.awaitMessage("oncommand");
  // Clear shortcut.
  inputEmpty.focus();
  EventUtils.synthesizeKey("KEY_Delete");
  await waitForShortcutSet(inputEmpty, "");
  EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  EventUtils.synthesizeKey("2", { altKey: true, shiftKey: true });
  is(
    await extension.awaitMessage("oncommand"),
    "commandTwo",
    "commandEmpty should be disabled, commandTwo should still be enabled"
  );

  // Now verify that the Backspace button does the same thing as Delete.
  inputEmpty.focus();
  EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  await waitForShortcutSet(inputEmpty, "Alt+Shift+3");
  inputEmpty.focus();
  EventUtils.synthesizeKey("KEY_Backspace");
  await waitForShortcutSet(input, "");
  EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  EventUtils.synthesizeKey("2", { altKey: true, shiftKey: true });
  is(
    await extension.awaitMessage("oncommand"),
    "commandTwo",
    "commandEmpty should be disabled again by Backspace"
  );

  // Check that the remove button works as expected.
  let inputTwo = getShortcutByName(doc, extension, "commandTwo");
  is(inputTwo.getAttribute("shortcut"), "Shift+Alt+2", "initial shortcut");
  checkHasRemoveButton(inputTwo, true);
  removeButtonForInput(inputTwo).click();
  is(inputTwo.getAttribute("shortcut"), "", "cleared shortcut");
  checkHasRemoveButton(inputTwo, false);
  ok(doc.activeElement != inputTwo, "input of removed shortcut is not focused");

  await closeShortcutsView(doc);

  await extension.unload();
});
