/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global Cu, BrowserTestUtils, is, ok, add_task, gBrowser */
"use strict";
Cu.import("resource://gre/modules/PromiseMessage.jsm", this);


const url = "http://example.org/tests/dom/manifest/test/resource.sjs";

/**
 * Test basic API error conditions
 */
add_task(function* () {
  yield BrowserTestUtils.withNewTab({gBrowser, url}, testPromiseMessageAPI)
});

function* testPromiseMessageAPI(aBrowser) {
  // Reusing an existing message.
  const msgKey = "DOM:WebManifest:hasManifestLink";
  const mm = aBrowser.messageManager;
  const id = "this should not change";
  const foo = "neitherShouldThis";
  const data = {id, foo};

  // This just returns false, and it doesn't matter for this test.
  yield PromiseMessage.send(mm, msgKey, data);

  // Check that no new props were added
  const props = Object.getOwnPropertyNames(data);
  ok(props.length === 2, "There should only be 2 props");
  ok(props.includes("id"), "Has the id property");
  ok(props.includes("foo"), "Has the foo property");

  // Check that the props didn't change.
  is(data.id, id, "The id prop must not change.");
  is(data.foo, foo, "The foo prop must not change.");
}
