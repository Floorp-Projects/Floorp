/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const hiddenPdfIds = ["backgrounds", "source-version-selection"];

async function checkElements({ removed, file, testName }) {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    for (let id of hiddenPdfIds) {
      is(
        !helper.get(id),
        removed,
        `${id} is ${removed ? "" : "not "}removed (${testName})`
      );
    }

    await helper.closeDialog();
  }, file);
}

add_task(async function testSettingsShownForNonPdf() {
  await checkElements({ removed: false, testName: "non-pdf" });
});

add_task(async function testSettingsHiddenForPdf() {
  await checkElements({
    removed: true,
    file: "file_pdf.pdf",
    testName: "pdf",
  });
});
