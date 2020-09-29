/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

// Get a ref to the pdf we want to open.
const PDF_URL = TESTROOT + "file_pdfjs_object_stream.pdf";

add_task(async function test_find_octet_stream_pdf() {
  await BrowserTestUtils.withNewTab(PDF_URL, async browser => {
    let findEls = ["cmd_find", "cmd_findAgain", "cmd_findPrevious"].map(id =>
      document.getElementById(id)
    );
    for (let el of findEls) {
      ok(!el.hasAttribute("disabled"), `${el.id} should be enabled`);
    }
  });
});
