/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This tests taskbar jump list functionality available on win7 and up.

const Cc = Components.classes;
const Ci = Components.interfaces;

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

  do_check_false(item.equals(sep));
  do_check_false(item.equals(shortcut));
  do_check_false(item.equals(link));

  do_check_false(sep.equals(item));
  do_check_false(sep.equals(shortcut));
  do_check_false(sep.equals(link));

  do_check_false(shortcut.equals(item));
  do_check_false(shortcut.equals(sep));
  do_check_false(shortcut.equals(link));

  do_check_false(link.equals(item));
  do_check_false(link.equals(sep));
  do_check_false(link.equals(shortcut));

  do_check_true(item.equals(item));
  do_check_true(sep.equals(sep));
  do_check_true(link.equals(link));
  do_check_true(shortcut.equals(shortcut));
}

function test_separator()
{
  // separators:

  var item = Cc["@mozilla.org/windows-jumplistseparator;1"].
  createInstance(Ci.nsIJumpListSeparator);

  do_check_true(item.type == Ci.nsIJumpListItem.JUMPLIST_ITEM_SEPARATOR);
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

  do_check_true(link.compareHash(uri2))
  uri2.spec = "http://www.456.com/";
  do_check_false(link.compareHash(uri2))
  uri2.spec = "http://www.123.com/";
  do_check_true(link.compareHash(uri2))
  uri2.spec = "https://www.123.com/";
  do_check_false(link.compareHash(uri2))
  uri2.spec = "http://www.123.com/test/";
  do_check_false(link.compareHash(uri2))
  uri1.spec = "http://www.123.com/test/";
  uri2.spec = "http://www.123.com/test/";
  do_check_true(link.compareHash(uri2))
  uri1.spec = "https://www.123.com/test/";
  uri2.spec = "https://www.123.com/test/";
  do_check_true(link.compareHash(uri2))
  uri2.spec = "ftp://www.123.com/test/";
  do_check_false(link.compareHash(uri2))
  uri2.spec = "http://123.com/test/";
  do_check_false(link.compareHash(uri2))
  uri1.spec = "https://www.123.com/test/";
  uri2.spec = "https://www.123.com/Test/";
  do_check_false(link.compareHash(uri2))

  uri1.spec = "http://www.123.com/";
  do_check_eq(link.uriHash, "QGLmWuwuTozr3tOfXSf5mg==");
  uri1.spec = "http://www.123.com/test/";
  do_check_eq(link.uriHash, "AG87Ls+GmaUYSUJFETRr3Q==");
  uri1.spec = "https://www.123.com/";
  do_check_eq(link.uriHash, "iSx6UH1a9enVPzUA9JZ42g==");

  var uri3 = Cc["@mozilla.org/network/simple-uri;1"]
            .createInstance(Ci.nsIURI);
  link.uri = uri3;
  do_check_eq(link.uriHash, "hTrpDwNRMkvXPqYV5kh1Fw==");
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

  do_check_true(link1.equals(link2));

  link2.uriTitle = "Testing";

  do_check_false(link1.equals(link2));

  link2.uriTitle = "Test";
  uri2.spec = "http://www.testing.com/";

  do_check_false(link1.equals(link2));
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
  do_check_eq(sc.iconIndex, 1);

  var iconImageUri = Cc["@mozilla.org/network/simple-uri;1"]
                    .createInstance(Ci.nsIURI);
  iconImageUri.spec = "http://www.123.com/";
  sc.iconImageUri = iconImageUri;
  do_check_eq(sc.iconImageUri, iconImageUri);

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                  getService(Ci.nsIProperties).
                  QueryInterface(Ci.nsIDirectoryService);
  var notepad = dirSvc.get("WinD", Ci.nsIFile);
  notepad.append("notepad.exe");
  if (notepad.exists()) {
    handlerApp.executable = notepad;
    sc.app = handlerApp;
    do_check_eq(sc.app.detailedDescription, "TestApp detailed description.");
    do_check_eq(sc.app.name, "TestApp");
    do_check_true(sc.app.parameterExists("-test"));
    do_check_false(sc.app.parameterExists("-notset"));
  }
}

function test_jumplist()
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

  do_check_neq(builder, null);

  // Win7 and up only
  try {
    var sysInfo = Cc["@mozilla.org/system-info;1"].
                  getService(Ci.nsIPropertyBag2);
    var ver = parseFloat(sysInfo.getProperty("version"));
    if (ver < 6.1) {
      do_check_false(builder.available, false);
      return;
    }
  } catch (ex) { }

  do_check_true(taskbar.available);

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
    handlerApp.executable = notepad;
    sc.app = handlerApp;
    items.appendElement(sc, false);

    var removed = Cc["@mozilla.org/array;1"]
                  .createInstance(Ci.nsIMutableArray);
    do_check_true(builder.initListBuild(removed));
    do_check_true(builder.addListToBuild(builder.JUMPLIST_CATEGORY_TASKS, items));
    do_check_true(builder.addListToBuild(builder.JUMPLIST_CATEGORY_RECENT));
    do_check_true(builder.addListToBuild(builder.JUMPLIST_CATEGORY_FREQUENT));
    do_check_true(builder.commitListBuild());

    builder.deleteActiveList();

    do_check_true(builder.initListBuild(removed));
    do_check_true(builder.addListToBuild(builder.JUMPLIST_CATEGORY_CUSTOM, items, "Custom List"));
    do_check_true(builder.commitListBuild());

    builder.deleteActiveList();
  }
}

function run_test()
{
  var isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
  if (!isWindows)
    return;

  test_basics();
  test_separator();
  test_hashes();
  test_links();
  test_shortcuts();
  test_jumplist();
}
