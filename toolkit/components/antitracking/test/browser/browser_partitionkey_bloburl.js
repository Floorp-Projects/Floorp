const BASE_URI =
  "https://example.net/browser/toolkit/components/antitracking/test/browser/blobPartitionPage.html";
const EMPTY_URI =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "https://example.com/browser/toolkit/components/antitracking/test/browser/empty.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.partition.bloburl_per_partition_key", true]],
  });
});

// Ensuring Blob URL cannot be resolved under a different
// top-level domain other than its original creation top-level domain
add_task(async function test_different_tld_with_iframe() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  let blobURL = await SpecialPowers.spawn(browser1, [], function () {
    return content.URL.createObjectURL(new content.Blob(["hello world!"]));
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, EMPTY_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  await SpecialPowers.spawn(
    browser2,
    [
      {
        page: BASE_URI,
        blob: blobURL,
      },
    ],
    async obj => {
      let ifr = content.document.createElement("iframe");
      ifr.setAttribute("id", "ifr");
      ifr.setAttribute("src", obj.page);

      info("Iframe loading...");
      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
      });

      let value = await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            resolve(e.data == "error");
          },
          { once: true }
        );
        ifr.contentWindow.postMessage(obj.blob, "*");
      });

      ok(value, "Blob URL was unable to be resolved");
    }
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Ensuring if Blob URL can be resolved if a domain1 creates a blob URL
// and domain1 trys to resolve blob URL within an iframe of itself
add_task(async function test_same_tld_with_iframe() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  let blobURL = await SpecialPowers.spawn(browser1, [], function () {
    return content.URL.createObjectURL(new content.Blob(["hello world!"]));
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  await SpecialPowers.spawn(
    browser2,
    [
      {
        page: BASE_URI,
        blob: blobURL,
      },
    ],
    async obj => {
      let ifr = content.document.createElement("iframe");
      ifr.setAttribute("id", "ifr");
      ifr.setAttribute("src", obj.page);

      info("Iframe loading...");
      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
      });

      let value = await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            resolve(e.data == "hello world!");
          },
          { once: true }
        );
        ifr.contentWindow.postMessage(obj.blob, "*");
      });

      ok(value, "Blob URL was able to be resolved");
    }
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Ensuring Blob URL can be resolved in an iframe
// under the same top-level domain where it creates.
add_task(async function test_no_iframes_same_tld() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let blobURL = await SpecialPowers.spawn(browser1, [], function () {
    return content.URL.createObjectURL(new content.Blob(["hello world!"]));
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(
    browser2,
    [blobURL],
    function (blobURL) {
      return new content.Promise(resolve => {
        var xhr = new content.XMLHttpRequest();
        xhr.open("GET", blobURL);
        xhr.onloadend = function () {
          resolve(xhr.response == "hello world!");
        };

        xhr.send();
      });
    }
  );

  ok(status, "Blob URL was able to be resolved");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Ensuring Blob URL can be resolved in a sandboxed
// iframe under the top-level domain where it creates.
add_task(async function test_same_tld_with_iframe() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  let blobURL = await SpecialPowers.spawn(browser1, [], function () {
    return content.URL.createObjectURL(new content.Blob(["hello world!"]));
  });

  await SpecialPowers.spawn(
    browser1,
    [
      {
        page: BASE_URI,
        blob: blobURL,
      },
    ],
    async obj => {
      let ifr = content.document.createElement("iframe");
      ifr.setAttribute("id", "ifr");
      ifr.setAttribute("sandbox", "allow-scripts allow-same-origin");
      ifr.setAttribute("src", obj.page);

      info("Iframe loading...");
      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
      });

      let value = await new content.Promise(resolve => {
        content.addEventListener(
          "message",
          e => {
            resolve(e.data == "hello world!");
          },
          { once: true }
        );
        ifr.contentWindow.postMessage(obj.blob, "*");
      });

      ok(value, "Blob URL was able to be resolved");
    }
  );

  BrowserTestUtils.removeTab(tab1);
});
