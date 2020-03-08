add_task(async function setupPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.documentchannel", true]],
  });
});

add_task(async function oopProcessSwap() {
  const FILE = fileURL("dummy_page.html");
  const WEB = httpURL("file_postmsg_parent.html");

  let win = await BrowserTestUtils.openNewBrowserWindow({ fission: true });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: FILE },
    async browser => {
      is(browser.browsingContext.children.length, 0);

      info("creating an in-process frame");
      let frameId = await SpecialPowers.spawn(
        browser,
        [{ FILE }],
        async ({ FILE }) => {
          let iframe = content.document.createElement("iframe");
          iframe.setAttribute("src", FILE);
          content.document.body.appendChild(iframe);

          // The nested URI should be same-process
          ok(iframe.browsingContext.docShell, "Should be in-process");

          return iframe.browsingContext.id;
        }
      );

      is(browser.browsingContext.children.length, 1);

      info("navigating to x-process frame");
      let oopinfo = await SpecialPowers.spawn(
        browser,
        [{ WEB }],
        async ({ WEB }) => {
          let iframe = content.document.querySelector("iframe");

          iframe.contentWindow.location = WEB;

          let data = await new Promise(resolve => {
            content.window.addEventListener(
              "message",
              function(evt) {
                info("oop iframe loaded");
                is(evt.source, iframe.contentWindow);
                resolve(evt.data);
              },
              { once: true }
            );
          });

          is(iframe.browsingContext.docShell, null, "Should be out-of-process");
          is(
            iframe.browsingContext.embedderElement,
            iframe,
            "correct embedder"
          );

          return {
            location: data.location,
            browsingContextId: iframe.browsingContext.id,
          };
        }
      );

      is(browser.browsingContext.children.length, 1);

      if (Services.prefs.getBoolPref("fission.preserve_browsing_contexts")) {
        is(
          frameId,
          oopinfo.browsingContextId,
          `BrowsingContext should not have changed (${frameId} != ${oopinfo.browsingContextId})`
        );
      }
      is(oopinfo.location, WEB, "correct location");
    }
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function oopOriginProcessSwap() {
  const COM_DUMMY = httpURL("dummy_page.html", "https://example.com/");
  const ORG_POSTMSG = httpURL(
    "file_postmsg_parent.html",
    "https://example.org/"
  );

  let win = await BrowserTestUtils.openNewBrowserWindow({ fission: true });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: COM_DUMMY },
    async browser => {
      is(browser.browsingContext.children.length, 0);

      info("creating an in-process frame");
      let frameId = await SpecialPowers.spawn(
        browser,
        [{ COM_DUMMY }],
        async ({ COM_DUMMY }) => {
          let iframe = content.document.createElement("iframe");
          iframe.setAttribute("src", COM_DUMMY);
          content.document.body.appendChild(iframe);

          // The nested URI should be same-process
          ok(iframe.browsingContext.docShell, "Should be in-process");

          return iframe.browsingContext.id;
        }
      );

      is(browser.browsingContext.children.length, 1);

      info("navigating to x-process frame");
      let oopinfo = await SpecialPowers.spawn(
        browser,
        [{ ORG_POSTMSG }],
        async ({ ORG_POSTMSG }) => {
          let iframe = content.document.querySelector("iframe");

          iframe.contentWindow.location = ORG_POSTMSG;

          let data = await new Promise(resolve => {
            content.window.addEventListener(
              "message",
              function(evt) {
                info("oop iframe loaded");
                is(evt.source, iframe.contentWindow);
                resolve(evt.data);
              },
              { once: true }
            );
          });

          is(iframe.browsingContext.docShell, null, "Should be out-of-process");
          is(
            iframe.browsingContext.embedderElement,
            iframe,
            "correct embedder"
          );

          return {
            location: data.location,
            browsingContextId: iframe.browsingContext.id,
          };
        }
      );

      is(browser.browsingContext.children.length, 1);
      if (Services.prefs.getBoolPref("fission.preserve_browsing_contexts")) {
        is(
          frameId,
          oopinfo.browsingContextId,
          `BrowsingContext should not have changed (${frameId} != ${oopinfo.browsingContextId})`
        );
      }
      is(oopinfo.location, ORG_POSTMSG, "correct location");
    }
  );

  await BrowserTestUtils.closeWindow(win);
});
