"use strict";

add_task(function*() {
  const PAGE_URL = getRootDirectory(gTestPath) + "file_viewsource.html";
  let viewSourceTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "view-source:" + PAGE_URL);

  let xhrPromise = new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", PAGE_URL, true);
    xhr.onload = event => resolve(event.target.responseText);
    xhr.send();
  });

  let viewSourceContentPromise = ContentTask.spawn(viewSourceTab.linkedBrowser, null, function*() {
    return content.document.body.textContent;
  });

  let results = yield Promise.all([viewSourceContentPromise, xhrPromise]);
  is(results[0], results[1], "Sources should match");
  yield BrowserTestUtils.removeTab(viewSourceTab);
});

