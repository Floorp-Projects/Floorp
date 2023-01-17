/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const UCT_URI = "chrome://mozapps/content/downloads/unknownContentType.xhtml";

async function promiseDownloadFinished(list) {
  return new Promise(resolve => {
    list.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or errored");
          list.removeView(this);
          resolve(download);
        }
      },
    });
  });
}

/**
 * Check that both in the download "what do you want to do with this file"
 * dialog and in the about:downloads download list, we represent blob URL
 * download sources using the principal (origin) that generated the blob.
 */
add_task(async function test_check_blob_origin_representation() {
  forcePromptForFiles("text/plain", "txt");

  await check_blob_origin(
    "https://example.org/1",
    "example.org",
    "example.org"
  );
  await check_blob_origin(
    "data:text/html,<body>Some Text<br>",
    "(data)",
    "blob"
  );
});

async function check_blob_origin(pageURL, expectedSource, expectedListOrigin) {
  await BrowserTestUtils.withNewTab(pageURL, async browser => {
    // Ensure we wait for the download to finish:
    let downloadList = await Downloads.getList(Downloads.PUBLIC);
    let downloadPromise = promiseDownloadFinished(downloadList);

    // Wait for the download prompting dialog
    let dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(
      null,
      win => win.document.documentURI == UCT_URI
    );

    // create and click an <a download> link to a txt file.
    await SpecialPowers.spawn(browser, [], () => {
      // Use `eval` to get a blob URL scoped to content, so that content is
      // actually allowed to open it and so we can check the origin is correct.
      let url = content.eval(`
        window.foo = new Blob(["Hello"], {type: "text/plain"});
        URL.createObjectURL(window.foo)`);
      let link = content.document.createElement("a");
      link.href = url;
      link.textContent = "Click me, click me, me me me";
      link.download = "my-file.txt";
      content.document.body.append(link);
      link.click();
    });

    // Check what we display in the dialog
    let dialogWin = await dialogPromise;
    let source = dialogWin.document.getElementById("source");
    is(
      source.value,
      expectedSource,
      "Should list origin as source if available."
    );

    // Close the dialog
    let closedPromise = BrowserTestUtils.windowClosed(dialogWin);
    // Ensure we're definitely saving (otherwise this depends on mime service
    // defaults):
    dialogWin.document.getElementById("save").click();
    let dialogNode = dialogWin.document.querySelector("dialog");
    dialogNode.getButton("accept").disabled = false;
    dialogNode.acceptDialog();
    await closedPromise;

    // Wait for the download to finish and ensure it is cleared up.
    let download = await downloadPromise;
    registerCleanupFunction(async () => {
      let target = download.target.path;
      await download.finalize();
      await IOUtils.remove(target);
    });

    // Check that the same download is displayed correctly in about:downloads.
    await BrowserTestUtils.withNewTab("about:downloads", async dlBrowser => {
      let doc = dlBrowser.contentDocument;
      let listNode = doc.getElementById("downloadsListBox");
      await BrowserTestUtils.waitForMutationCondition(
        listNode,
        { childList: true, subtree: true, attributeFilter: ["value"] },
        () =>
          listNode.firstElementChild
            ?.querySelector(".downloadDetailsNormal")
            ?.getAttribute("value")
      );
      let download = listNode.firstElementChild;
      let detailString = download.querySelector(".downloadDetailsNormal").value;
      Assert.stringContains(
        detailString,
        expectedListOrigin,
        "Should list origin in download list if available."
      );
    });
  });
}
