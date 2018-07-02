"use strict";

const server = createHttpServer({hosts: ["example.org"]});
server.registerPathHandler("/sameSiteCookiesApiTest", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_samesite_cookies() {
  function contentScript() {
    document.cookie = "test1=whatever";
    document.cookie = "test2=whatever; SameSite=lax";
    document.cookie = "test3=whatever; SameSite=strict";
    browser.runtime.sendMessage("do-check-cookies");
  }
  async function background() {
    await new Promise(resolve => {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq("do-check-cookies", msg, "expected message");
        resolve();
      });
    });

    const url = "https://example.org/";

    // Baseline. Every cookie must have the expected sameSite.
    let cookie = await browser.cookies.get({url, name: "test1"});
    browser.test.assertEq("no_restriction", cookie.sameSite, "Expected sameSite for test1");

    cookie = await browser.cookies.get({url, name: "test2"});
    browser.test.assertEq("lax", cookie.sameSite, "Expected sameSite for test2");

    cookie = await browser.cookies.get({url, name: "test3"});
    browser.test.assertEq("strict", cookie.sameSite, "Expected sameSite for test3");

    // Testing cookies.getAll + cookies.set
    let cookies = await browser.cookies.getAll({url, name: "test3"});
    browser.test.assertEq(1, cookies.length, "There is only one test3 cookie");

    cookie = await browser.cookies.set({url, name: "test3", value: "newvalue"});
    browser.test.assertEq("no_restriction", cookie.sameSite, "sameSite defaults to no_restriction");

    for (let sameSite of ["no_restriction", "lax", "strict"]) {
      cookie = await browser.cookies.set({url, name: "test3", sameSite});
      browser.test.assertEq(sameSite, cookie.sameSite, `Expected sameSite=${sameSite} in return value of cookies.set`);
      cookies = await browser.cookies.getAll({url, name: "test3"});
      browser.test.assertEq(1, cookies.length, `test3 is still the only cookie after setting sameSite=${sameSite}`);
      browser.test.assertEq(sameSite, cookies[0].sameSite, `test3 was updated to sameSite=${sameSite}`);
    }

    browser.test.notifyPass("cookies");
  }
  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["cookies", "*://example.org/"],
      content_scripts: [{
        matches: ["*://example.org/sameSiteCookiesApiTest*"],
        js: ["contentscript.js"],
      }],
    },
    files: {
      "contentscript.js": contentScript,
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.org/sameSiteCookiesApiTest");
  await extension.awaitFinish("cookies");
  await contentPage.close();
  await extension.unload();
});
