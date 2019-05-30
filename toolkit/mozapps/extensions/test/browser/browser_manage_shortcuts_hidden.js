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

async function registerAndStartExtension(mockProvider, ext) {
  // Shortcuts are registered when an extension is started, so we need to load
  // and start an extension.
  let extension = ExtensionTestUtils.loadExtension(ext);
  await extension.startup();

  // Extensions only appear in the add-on manager when they are registered with
  // the add-on manager, e.g. by passing "useAddonManager" to `loadExtension`.
  // "useAddonManager" can however not be used, because the resulting add-ons
  // are unsigned, and only add-ons with privileged signatures can be hidden.
  mockProvider.createAddons([{
    id: extension.id,
    name: ext.manifest.name,
    type: "extension",
    version: "1",
    // We use MockProvider because the "hidden" property cannot
    // be set when "useAddonManager" is passed to loadExtension.
    hidden: ext.manifest.hidden,
  }]);
  return extension;
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

// Hidden add-ons without shortcuts should be hidden,
// but their card should be shown if there is a shortcut.
add_task(async function hidden_extension() {
  let mockProvider = new MockProvider();
  let hiddenExt1 = await registerAndStartExtension(mockProvider, {
    manifest: {
      name: "hidden with shortcuts",
      hidden: true,
      commands: {
        hiddenShortcut: {},
      },
    },
  });
  let hiddenExt2 = await registerAndStartExtension(mockProvider, {
    manifest: {
      name: "hidden without shortcuts",
      hidden: true,
    },
  });

  let doc = await loadShortcutsView();

  ok(getShortcutByName(doc, hiddenExt1, "hiddenShortcut"),
     "Hidden extension with shortcuts should have a card");

  is(getShortcutCard(doc, hiddenExt2), null,
     "Hidden extension without shortcuts should not have a card");
  is(getNoShortcutListItem(doc, hiddenExt2), null,
     "Hidden extension without shortcuts should not be listed");

  await closeShortcutsView(doc);
  await hiddenExt1.unload();
  await hiddenExt2.unload();

  mockProvider.unregister();
});
