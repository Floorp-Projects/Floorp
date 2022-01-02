/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_FormFieldLabels() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let fields = Array.from(helper.get("print").elements);
    for (let field of fields) {
      if (field.localName == "button") {
        continue;
      }
      ok(
        field.labels.length ||
          field.hasAttribute("aria-label") ||
          field.hasAttribute("aria-labelledby"),
        `Field ${field.localName}#${field.id} should be labelled`
      );
    }
  });
});
