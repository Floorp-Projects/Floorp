/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests taskbar jump list functionality available on win7 and up.

var Cc = Components.classes;
var Ci = Components.interfaces;

function test_basics()
{
  var item = Cc["@mozilla.org/windows-jumplistitem;1"].
  createInstance(Ci.nsIJumpListItem);

  var sep = Cc["@mozilla.org/windows-jumplistseparator;1"].
  createInstance(Ci.nsIJumpListSeparator);

  var shortcut = Cc["@mozilla.org/windows-jumplistshortcut;1"].
  createInstance(Ci.nsIJumpListShortcut);

  var link = Cc["@mozilla.org/windows-jumplistlink;1"].
  createInstance(Ci.nsIJumpListLink);

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

function test_separator()
{
  // separators:

  var item = Cc["@mozilla.org/windows-jumplistseparator;1"].
  createInstance(Ci.nsIJumpListSeparator);

  Assert.ok(item.type == Ci.nsIJumpListItem.JUMPLIST_ITEM_SEPARATOR);
}

function test_hashes()
{
  var link = Cc["@mozilla.org/windows-jumplistlink;1"]
             .createInstance(Ci.nsIJumpListLink);
  var uri1 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);
  var uri2 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);

  uri1.spec = "http://www.123.com/";
  uri2.spec = "http://www.123.com/";

  link.uri = uri1;

  Assert.ok(link.compareHash(uri2))
  uri2.spec = "http://www.456.com/";
  Assert.ok(!link.compareHash(uri2))
  uri2.spec = "http://www.123.com/";
  Assert.ok(link.compareHash(uri2))
  uri2.spec = "https://www.123.com/";
  Assert.ok(!link.compareHash(uri2))
  uri2.spec = "http://www.123.com/test/";
  Assert.ok(!link.compareHash(uri2))
  uri1.spec = "http://www.123.com/test/";
  uri2.spec = "http://www.123.com/test/";
  Assert.ok(link.compareHash(uri2))
  uri1.spec = "https://www.123.com/test/";
  uri2.spec = "https://www.123.com/test/";
  Assert.ok(link.compareHash(uri2))
  uri2.spec = "ftp://www.123.com/test/";
  Assert.ok(!link.compareHash(uri2))
  uri2.spec = "http://123.com/test/";
  Assert.ok(!link.compareHash(uri2))
  uri1.spec = "https://www.123.com/test/";
  uri2.spec = "https://www.123.com/Test/";
  Assert.ok(!link.compareHash(uri2))

  uri1.spec = "http://www.123.com/";
  Assert.equal(link.uriHash, "QGLmWuwuTozr3tOfXSf5mg==");
  uri1.spec = "http://www.123.com/test/";
  Assert.equal(link.uriHash, "AG87Ls+GmaUYSUJFETRr3Q==");
  uri1.spec = "https://www.123.com/";
  Assert.equal(link.uriHash, "iSx6UH1a9enVPzUA9JZ42g==");

  var uri3 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);
  link.uri = uri3;
  Assert.equal(link.uriHash, "hTrpDwNRMkvXPqYV5kh1Fw==");
}

function test_links()
{
  // links:
  var link1 = Cc["@mozilla.org/windows-jumplistlink;1"]
             .createInstance(Ci.nsIJumpListLink);
  var link2 = Cc["@mozilla.org/windows-jumplistlink;1"]
              .createInstance(Ci.nsIJumpListLink);

  var uri1 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);
  var uri2 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);

  uri1.spec = "http://www.test.com/";
  uri2.spec = "http://www.test.com/";

  link1.uri = uri1;
  link1.uriTitle = "Test";
  link2.uri = uri2;
  link2.uriTitle = "Test";

  Assert.ok(link1.equals(link2));

  link2.uriTitle = "Testing";

  Assert.ok(!link1.equals(link2));

  link2.uriTitle = "Test";
  uri2.spec = "http://www.testing.com/";

  Assert.ok(!link1.equals(link2));
}

function test_shortcuts()
{
  // shortcuts:
  var sc = Cc["@mozilla.org/windows-jumplistshortcut;1"]
           .createInstance(Ci.nsIJumpListShortcut);

  var handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"]
                   .createInstance(Ci.nsILocalHandlerApp);

  handlerApp.name = "TestApp";
  handlerApp.detailedDescription = "TestApp detailed description.";
  handlerApp.appendParameter("-test");

  sc.iconIndex = 1;
  Assert.equal(sc.iconIndex, 1);

  var faviconPageUri = Cc["@mozilla.org/network/simple-uri;1"]
                    .createInstance(Ci.nsIURI);
  faviconPageUri.spec = "http://www.123.com/";
  sc.faviconPageUri = faviconPageUri;
  Assert.equal(sc.faviconPageUri, faviconPageUri);

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                  getService(Ci.nsIProperties).
                  QueryInterface(Ci.nsIDirectoryService);
  var notepad = dirSvc.get("WinD", Ci.nsIFile);
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

async function test_jumplist()
{
  // Jump lists can't register links unless the application is the default
  // protocol handler for the protocol of the link, so we skip off testing
  // those in these tests. We'll init the jump list for the xpc shell harness,
  // add a task item, and commit it.
 
  // not compiled in
  if (Ci.nsIWinTaskbar == null)
    return;

  var taskbar = Cc["@mozilla.org/windows-taskbar;1"]
                .getService(Ci.nsIWinTaskbar);

  var builder = taskbar.createJumpListBuilder();

  Assert.notEqual(builder, null);

  // Win7 and up only
  try {
    var sysInfo = Cc["@mozilla.org/system-info;1"].
                  getService(Ci.nsIPropertyBag2);
    var ver = parseFloat(sysInfo.getProperty("version"));
    if (ver < 6.1) {
      Assert.ok(!builder.available, false);
      return;
    }
  } catch (ex) { }

  Assert.ok(taskbar.available);

  builder.deleteActiveList();

  var items = Cc["@mozilla.org/array;1"]
              .createInstance(Ci.nsIMutableArray);

  var sc = Cc["@mozilla.org/windows-jumplistshortcut;1"]
           .createInstance(Ci.nsIJumpListShortcut);

  var handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"]
                   .createInstance(Ci.nsILocalHandlerApp);

  handlerApp.name = "Notepad";
  handlerApp.detailedDescription = "Testing detailed description.";

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                  getService(Ci.nsIProperties).
                  QueryInterface(Ci.nsIDirectoryService);
  var notepad = dirSvc.get("WinD", Ci.nsIFile);
  notepad.append("notepad.exe");
  if (notepad.exists()) {
    // To ensure "profile-before-change" will fire before
    // "xpcom-shutdown-threads"
    do_get_profile();

    handlerApp.executable = notepad;
    sc.app = handlerApp;
    items.appendElement(sc);

    var removed = Cc["@mozilla.org/array;1"]
                  .createInstance(Ci.nsIMutableArray);
    Assert.ok(builder.initListBuild(removed));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_TASKS, items));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_RECENT));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_FREQUENT));
    let rv = new Promise((resolve) => {
      builder.commitListBuild(resolve);
    });
    Assert.ok(await rv);

    builder.deleteActiveList();

    Assert.ok(builder.initListBuild(removed));
    Assert.ok(builder.addListToBuild(builder.JUMPLIST_CATEGORY_CUSTOMLIST, items, "Custom List"));
    rv = new Promise((resolve) => {
      builder.commitListBuild(resolve);
    });
    Assert.ok(await rv);

    builder.deleteActiveList();
  }
}

function run_test()
{
  if (mozinfo.os != "win") {
    return;
  }
  test_basics();
  test_separator();
  test_hashes();
  test_links();
  test_shortcuts();

  run_next_test();
}

add_task(test_jumplist);
