/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that <Url rel="searchform"/> is properly recognized as a searchForm.
 */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_rel_searchform() {
  let engineNames = [
    "engine-rel-searchform.xml",
    "engine-rel-searchform-post.xml",
  ];

  // The final searchForm of the engine should be a URL whose domain is the
  // <ShortName> in the engine's XML and that has a ?search parameter.  The
  // point of the ?search parameter is to avoid accidentally matching the value
  // returned as a last resort by Engine's searchForm getter, which is simply
  // the prePath of the engine's first HTML <Url>.
  let items = engineNames.map(e => ({ name: e, xmlFileName: e }));
  for (let engine of await addTestEngines(items)) {
    Assert.equal(engine.searchForm, "http://" + engine.name + "/?search");
  }
});
