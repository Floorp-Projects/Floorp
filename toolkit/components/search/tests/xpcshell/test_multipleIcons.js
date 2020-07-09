/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getIcons() and getIconURLBySize() on engine with multiple icons.
 */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_multipleIcons() {
  let [engine] = await addTestEngines([
    { name: "IconsTest", xmlFileName: "engineImages.xml" },
  ]);

  info("The default should be the 16x16 icon");
  Assert.ok(engine.iconURI.spec.includes("ico16"));

  Assert.ok(engine.getIconURLBySize(16, 16).includes("ico16"));
  Assert.ok(engine.getIconURLBySize(32, 32).includes("ico32"));
  Assert.ok(engine.getIconURLBySize(74, 74).includes("ico74"));

  info("Invalid dimensions should return null.");
  Assert.equal(null, engine.getIconURLBySize(50, 50));

  let allIcons = engine.getIcons();

  info("Check that allIcons contains expected icon sizes");
  Assert.equal(allIcons.length, 3);
  let expectedWidths = [16, 32, 74];
  Assert.ok(
    allIcons.every(item => {
      let width = item.width;
      Assert.notEqual(expectedWidths.indexOf(width), -1);
      Assert.equal(width, item.height);

      let icon = item.url.split(",").pop();
      Assert.equal(icon, "ico" + width);

      return true;
    })
  );
});

add_task(async function test_icon_not_in_file() {
  let engineUrl = gDataUrl + "engine-fr.xml";
  let engine = await Services.search.addOpenSearchEngine(
    engineUrl,
    "data:image/x-icon;base64,ico16"
  );

  // Even though the icon wasn't specified inside the XML file, it should be
  // available both in the iconURI attribute and with getIconURLBySize.
  Assert.ok(engine.iconURI.spec.includes("ico16"));
  Assert.ok(engine.getIconURLBySize(16, 16).includes("ico16"));
});
