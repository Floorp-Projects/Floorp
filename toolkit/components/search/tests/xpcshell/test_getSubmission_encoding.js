/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefix = "https://example.com/?sourceId=Mozilla-search&search=";

add_setup(async function () {
  await SearchTestUtils.useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

function testEncode(engine, charset, query, expected) {
  engine.wrappedJSObject._queryCharset = charset;

  Assert.equal(
    engine.getSubmission(query).uri.spec,
    prefix + expected,
    `Should have correctly encoded for ${charset}`
  );
}

add_task(async function test_getSubmission_encoding() {
  let engine = await Services.search.getEngineByName("Simple Engine");

  testEncode(engine, "UTF-8", "caff\u00E8", "caff%C3%A8");
  testEncode(engine, "windows-1252", "caff\u00E8", "caff%E8");
});
