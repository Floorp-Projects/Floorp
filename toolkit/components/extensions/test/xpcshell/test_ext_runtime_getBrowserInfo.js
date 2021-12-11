/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function setup() {
  ExtensionTestUtils.mockAppInfo();
});

add_task(async function test_getBrowserInfo() {
  async function background() {
    let info = await browser.runtime.getBrowserInfo();

    browser.test.assertEq(info.name, "XPCShell", "name is valid");
    browser.test.assertEq(info.vendor, "Mozilla", "vendor is Mozilla");
    browser.test.assertEq(info.version, "48", "version is correct");
    browser.test.assertEq(info.buildID, "20160315", "buildID is correct");

    browser.test.notifyPass("runtime.getBrowserInfo");
  }

  const extension = ExtensionTestUtils.loadExtension({ background });
  await extension.startup();
  await extension.awaitFinish("runtime.getBrowserInfo");
  await extension.unload();
});
