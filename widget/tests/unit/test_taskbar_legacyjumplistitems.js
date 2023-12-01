/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests taskbar jump list functionality available on win7 and up.

function test_basics() {
  var item = Cc["@mozilla.org/windows-legacyjumplistitem;1"].createInstance(
    Ci.nsILegacyJumpListItem
  );

  var sep = Cc["@mozilla.org/windows-legacyjumplistseparator;1"].createInstance(
    Ci.nsILegacyJumpListSeparator
  );

  var shortcut = Cc[
    "@mozilla.org/windows-legacyjumplistshortcut;1"
  ].createInstance(Ci.nsILegacyJumpListShortcut);

  var link = Cc["@mozilla.org/windows-legacyjumplistlink;1"].createInstance(
    Ci.nsILegacyJumpListLink
  );

  Assert.ok(!item.equals(sep));
  Assert.ok(!item.equals(shortcut));
  Assert.ok(!item.equals(link));

  Assert.ok(!sep.equals(item));
  Assert.ok(!sep.equals(shortcut));
  Assert.ok(!sep.equals(link));

  Assert.ok(!shortcut.equals(item));
  Assert.ok(!shortcut.equals(sep));
  Assert.ok(!shortcut.equals(link));

  Assert.ok(!link.equals(item));
  Assert.ok(!link.equals(sep));
  Assert.ok(!link.equals(shortcut));

  Assert.ok(item.equals(item));
  Assert.ok(sep.equals(sep));
  Assert.ok(link.equals(link));
  Assert.ok(shortcut.equals(shortcut));
}

function test_separator() {
  // separators:

  var item = Cc[
    "@mozilla.org/windows-legacyjumplistseparator;1"
  ].createInstance(Ci.nsILegacyJumpListSeparator);

  Assert.ok(item.type == Ci.nsILegacyJumpListItem.JUMPLIST_ITEM_SEPARATOR);
}

function test_links() {
  // links:
  var link1 = Cc["@mozilla.org/windows-legacyjumplistlink;1"].createInstance(
    Ci.nsILegacyJumpListLink
  );
  var link2 = Cc["@mozilla.org/windows-legacyjumplistlink;1"].createInstance(
    Ci.nsILegacyJumpListLink
  );

  var uri1 = Cc["@mozilla.org/network/simple-uri-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("http://www.test.com/")
    .finalize();
  var uri2 = Cc["@mozilla.org/network/simple-uri-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("http://www.test.com/")
    .finalize();

  link1.uri = uri1;
  link1.uriTitle = "Test";
  link2.uri = uri2;
  link2.uriTitle = "Test";

  Assert.ok(link1.equals(link2));

  link2.uriTitle = "Testing";

  Assert.ok(!link1.equals(link2));

  link2.uriTitle = "Test";
  uri2 = uri2.mutate().setSpec("http://www.testing.com/").finalize();
  link2.uri = uri2;

  Assert.ok(!link1.equals(link2));
}

function test_shortcuts() {
  // shortcuts:
  var sc = Cc["@mozilla.org/windows-legacyjumplistshortcut;1"].createInstance(
    Ci.nsILegacyJumpListShortcut
  );

  var handlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);

  handlerApp.name = "TestApp";
  handlerApp.detailedDescription = "TestApp detailed description.";
  handlerApp.appendParameter("-test");

  sc.iconIndex = 1;
  Assert.equal(sc.iconIndex, 1);

  var faviconPageUri = Cc["@mozilla.org/network/simple-uri-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("http://www.123.com/")
    .finalize();
  sc.faviconPageUri = faviconPageUri;
  Assert.equal(sc.faviconPageUri, faviconPageUri);

  var notepad = Services.dirsvc.get("WinD", Ci.nsIFile);
  notepad.append("notepad.exe");
  if (notepad.exists()) {
    handlerApp.executable = notepad;
    sc.app = handlerApp;
    Assert.equal(sc.app.detailedDescription, "TestApp detailed description.");
    Assert.equal(sc.app.name, "TestApp");
    Assert.ok(sc.app.parameterExists("-test"));
    Assert.ok(!sc.app.parameterExists("-notset"));
  }
}

async function test_legacyjumplist() {
  // Jump lists can't register links unless the application is the default
  // protocol handler for the protocol of the link, so we skip off testing
  // those in these tests. We'll init the jump list for the xpc shell harness,
  // add a task item, and commit it.

  // not compiled in
  if (Ci.nsIWinTaskbar == null) {
    return;
  }

  var taskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
    Ci.nsIWinTaskbar
  );

  // Since we're only testing the general functionality of the JumpListBuilder
  // et. al, we can just test the non-private browsing version.
  // (The only difference between the two at this level is the App User Model ID.)
  var builder = taskbar.createLegacyJumpListBuilder(false);

  Assert.notEqual(builder, null);

  // Win7 and up only
  try {
    var ver = parseFloat(Services.sysinfo.getProperty("version"));
    if (ver < 6.1) {
      Assert.ok(!builder.available);
      return;
    }
  } catch (ex) {}

  Assert.ok(taskbar.available);

  builder.deleteActiveList();

  var items = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

  var sc = Cc["@mozilla.org/windows-legacyjumplistshortcut;1"].createInstance(
    Ci.nsILegacyJumpListShortcut
  );

  var handlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);

  handlerApp.name = "Notepad";
  handlerApp.detailedDescription = "Testing detailed description.";

  var notepad = Services.dirsvc.get("WinD", Ci.nsIFile);
  notepad.append("notepad.exe");
  if (notepad.exists()) {
    // To ensure "profile-before-change" will fire before
    // "xpcom-shutdown-threads"
    do_get_profile();

    handlerApp.executable = notepad;
    sc.app = handlerApp;
    items.appendElement(sc);

    var removed = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    Assert.ok(builder.initListBuild(removed));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_TASKS, items));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_RECENT));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_FREQUENT));
    let rv = new Promise(resolve => {
      builder.commitListBuild(resolve);
    });
    Assert.ok(await rv);

    builder.deleteActiveList();

    Assert.ok(builder.initListBuild(removed));
    Assert.ok(
      builder.addListToBuild(
        builder.JUMPLIST_CATEGORY_CUSTOMLIST,
        items,
        "Custom List"
      )
    );
    rv = new Promise(resolve => {
      builder.commitListBuild(resolve);
    });
    Assert.ok(await rv);

    builder.deleteActiveList();
  }
}

function run_test() {
  if (mozinfo.os != "win") {
    return;
  }
  test_basics();
  test_separator();
  test_links();
  test_shortcuts();

  run_next_test();
}

add_task(test_legacyjumplist);
