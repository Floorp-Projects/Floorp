"use strict";

add_task(async function() {
  const PAGE_URL = getRootDirectory(gTestPath) + "file_viewsource.html";
  let viewSourceTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "view-source:" + PAGE_URL);

  let xhrPromise = new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", PAGE_URL, true);
    xhr.onload = event => resolve(event.target.responseText);
    xhr.send();
  });

  let viewSourceContentPromise = ContentTask.spawn(viewSourceTab.linkedBrowser, null, async function() {
    return content.document.body.textContent;
  });

  let results = await Promise.all([viewSourceContentPromise, xhrPromise]);
  is(results[0], results[1], "Sources should match");
  await BrowserTestUtils.removeTab(viewSourceTab);
});

