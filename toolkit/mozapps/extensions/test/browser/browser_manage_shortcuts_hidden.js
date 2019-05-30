/* eslint max-len: ["error", 80] */
"use strict";

async function loadShortcutsView() {
  let managerWin = await open_manager(null);
  managerWin.gViewController.loadView("addons://shortcuts/shortcuts");
  await wait_for_view_load(managerWin);
  return managerWin.document.getElementById("shortcuts-view").contentDocument;
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

function getNoShortcutListItem(doc, extension) {
  let {id} = extension;
  let li = doc.querySelector(`.shortcuts-no-commands-list [addon-id="${id}"]`);
  return li && li.textContent;
}

add_task(async function extension_with_shortcuts() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "shortcut addon",
      commands: {
        theShortcut: {},
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let doc = await loadShortcutsView();

  ok(getShortcutByName(doc, extension, "theShortcut"),
     "Extension with shortcuts should have a card");
  is(getNoShortcutListItem(doc, extension), null,
     "Extension with shortcuts should not be listed");

  await closeShortcutsView(doc);
  await extension.unload();
});

add_task(async function extension_without_shortcuts() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "no shortcut addon",
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let doc = await loadShortcutsView();

  is(getShortcutCard(doc, extension), null,
     "Extension without shortcuts should not have a card");
  is(getNoShortcutListItem(doc, extension), "no shortcut addon",
     "The add-on's name is set in the list");

  await closeShortcutsView(doc);
  await extension.unload();
});
