/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getIcons() and getIconURLBySize() on engine with multiple icons.
 */

"use strict";

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_multipleIcons() {
  let [engine] = yield addTestEngines([
    { name: "IconsTest", xmlFileName: "engineImages.xml" },
  ]);

  do_print("The default should be the 16x16 icon");
  do_check_true(engine.iconURI.spec.includes("ico16"));

  do_check_true(engine.getIconURLBySize(16, 16).includes("ico16"));
  do_check_true(engine.getIconURLBySize(32, 32).includes("ico32"));
  do_check_true(engine.getIconURLBySize(74, 74).includes("ico74"));

  do_print("Invalid dimensions should return null.");
  do_check_null(engine.getIconURLBySize(50, 50));

  let allIcons = engine.getIcons();

  do_print("Check that allIcons contains expected icon sizes");
  do_check_eq(allIcons.length, 3);
  let expectedWidths = [16, 32, 74];
  do_check_true(allIcons.every((item) => {
    let width = item.width;
    do_check_neq(expectedWidths.indexOf(width), -1);
    do_check_eq(width, item.height);

    let icon = item.url.split(",").pop();
    do_check_eq(icon, "ico" + width);

    return true;
  }));
});

add_task(function* test_icon_not_in_file() {
  let engineUrl = gDataUrl + "engine-fr.xml";
  let engine = yield new Promise((resolve, reject) => {
    Services.search.addEngine(engineUrl, null, "data:image/x-icon;base64,ico16",
                              false, {onSuccess: resolve, onError: reject});
  });

  // Even though the icon wasn't specified inside the XML file, it should be
  // available both in the iconURI attribute and with getIconURLBySize.
  do_check_true(engine.iconURI.spec.includes("ico16"));
  do_check_true(engine.getIconURLBySize(16, 16).includes("ico16"));
});
