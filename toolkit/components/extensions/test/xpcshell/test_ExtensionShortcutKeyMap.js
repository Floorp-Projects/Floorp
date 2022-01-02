/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionShortcutKeyMap } = ChromeUtils.import(
  "resource://gre/modules/ExtensionShortcuts.jsm"
);

add_task(function test_ExtensionShortcutKeymap() {
  const shortcutsMap = new ExtensionShortcutKeyMap();

  shortcutsMap.recordShortcut("Ctrl+Shift+1", "Addon1", "Command1");
  shortcutsMap.recordShortcut("Ctrl+Shift+1", "Addon2", "Command2");
  shortcutsMap.recordShortcut("Ctrl+Alt+2", "Addon2", "Command3");
  // Empty shortcut not expected to be recorded, just ignored.
  shortcutsMap.recordShortcut("", "Addon3", "Command4");

  Assert.equal(
    shortcutsMap.size,
    2,
    "Got the expected number of shortcut entries"
  );
  Assert.deepEqual(
    {
      shortcutWithTwoExtensions: shortcutsMap.getFirstAddonName("Ctrl+Shift+1"),
      shortcutWithOnlyOneExtension: shortcutsMap.getFirstAddonName(
        "Ctrl+Alt+2"
      ),
      shortcutWithNoExtension: shortcutsMap.getFirstAddonName(""),
    },
    {
      shortcutWithTwoExtensions: "Addon1",
      shortcutWithOnlyOneExtension: "Addon2",
      shortcutWithNoExtension: null,
    },
    "Got the expected results from getFirstAddonName calls"
  );

  Assert.deepEqual(
    {
      shortcutWithTwoExtensions: shortcutsMap.has("Ctrl+Shift+1"),
      shortcutWithOnlyOneExtension: shortcutsMap.has("Ctrl+Alt+2"),
      shortcutWithNoExtension: shortcutsMap.has(""),
    },
    {
      shortcutWithTwoExtensions: true,
      shortcutWithOnlyOneExtension: true,
      shortcutWithNoExtension: false,
    },
    "Got the expected results from `has` calls"
  );

  shortcutsMap.removeShortcut("Ctrl+Shift+1", "Addon1", "Command1");
  Assert.equal(
    shortcutsMap.has("Ctrl+Shift+1"),
    true,
    "Expect shortcut to already exist after removing one duplicate"
  );
  Assert.equal(
    shortcutsMap.getFirstAddonName("Ctrl+Shift+1"),
    "Addon2",
    "Expect getFirstAddonName to return the remaining addon name"
  );

  shortcutsMap.removeShortcut("Ctrl+Shift+1", "Addon2", "Command2");
  Assert.equal(
    shortcutsMap.has("Ctrl+Shift+1"),
    false,
    "Expect shortcut to not exist anymore after removing last entry"
  );
  Assert.equal(shortcutsMap.size, 1, "Got only one shortcut as expected");

  shortcutsMap.clear();
  Assert.equal(
    shortcutsMap.size,
    0,
    "Got no shortcut as expected after clearing the map"
  );
});

// This test verify that ExtensionShortcutKeyMap does catch duplicated
// shortcut when the two modifiers strings are associated to the same
// key (in particular on macOS where Ctrl and Command keys are both translated
// in the same modifier in the keyboard shortcuts).
add_task(function test_PlatformShortcutString() {
  const shortcutsMap = new ExtensionShortcutKeyMap();

  // Make the class instance behave like it would while running on macOS.
  // (this is just for unit testing purpose, there is a separate integration
  // test exercising this behavior in a real "Manage Extension Shortcut"
  // about:addons view and only running on macOS, skipped on other platforms).
  shortcutsMap._os = "mac";

  shortcutsMap.recordShortcut("Ctrl+Shift+1", "Addon1", "MacCommand1");

  Assert.deepEqual(
    {
      hasWithCtrl: shortcutsMap.has("Ctrl+Shift+1"),
      hasWithCommand: shortcutsMap.has("Command+Shift+1"),
    },
    {
      hasWithCtrl: true,
      hasWithCommand: true,
    },
    "Got the expected results from `has` calls"
  );

  Assert.deepEqual(
    {
      nameWithCtrl: shortcutsMap.getFirstAddonName("Ctrl+Shift+1"),
      nameWithCommand: shortcutsMap.getFirstAddonName("Command+Shift+1"),
    },
    {
      nameWithCtrl: "Addon1",
      nameWithCommand: "Addon1",
    },
    "Got the expected results from `getFirstAddonName` calls"
  );

  // Add a duplicate shortcut using Command instead of Ctrl and
  // verify the expected behaviors.
  shortcutsMap.recordShortcut("Command+Shift+1", "Addon2", "MacCommand2");
  Assert.equal(shortcutsMap.size, 1, "Got still one shortcut as expected");
  shortcutsMap.removeShortcut("Ctrl+Shift+1", "Addon1", "MacCommand1");
  Assert.equal(shortcutsMap.size, 1, "Got still one shortcut as expected");
  Assert.deepEqual(
    {
      nameWithCtrl: shortcutsMap.getFirstAddonName("Ctrl+Shift+1"),
      nameWithCommand: shortcutsMap.getFirstAddonName("Command+Shift+1"),
    },
    {
      nameWithCtrl: "Addon2",
      nameWithCommand: "Addon2",
    },
    "Got the expected results from `getFirstAddonName` calls"
  );

  // Remove the entry added with a shortcut using "Command" by using the
  // equivalent shortcut using Ctrl.
  shortcutsMap.removeShortcut("Ctrl+Shift+1", "Addon2", "MacCommand2");
  Assert.equal(shortcutsMap.size, 0, "Got no shortcut as expected");
});
