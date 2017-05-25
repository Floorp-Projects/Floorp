/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async function() {
  info("Make sure navigation through links in resource:// pages work");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "resource://gre/" }, async function(browser) {
    // Following a directory link shall properly open the directory (bug 1224046)
    await ContentTask.spawn(browser, {}, function() {
      let link = Array.prototype.filter.call(content.document.getElementsByClassName('dir'), function(element) {
        let name = element.textContent;
        // Depending whether resource:// is backed by jar: or file://,
        // directories either have a trailing slash or they don't.
        if (name.endsWith("/")) {
          name = name.slice(0, -1);
        }
        return name == "components";
      })[0];
      // First ensure the link is in the viewport
      link.scrollIntoView();
      // Then click on it.
      link.click();
    });

    await BrowserTestUtils.browserLoaded(browser, undefined, "resource://gre/components/");

    // Following the parent link shall properly open the parent (bug 1366180)
    await ContentTask.spawn(browser, {}, function() {
      let link = content.document.getElementById('UI_goUp').getElementsByTagName('a')[0];
      // The link should always be high enough in the page to be in the viewport.
      link.click();
    });

    await BrowserTestUtils.browserLoaded(browser, undefined, "resource://gre/");

    // Following a link to a given file shall properly open the file.
    await ContentTask.spawn(browser, {}, function() {
      let link = Array.prototype.filter.call(content.document.getElementsByClassName('file'), function(element) {
        return element.textContent == "greprefs.js";
      })[0];
      link.scrollIntoView();
      link.click();
    });

    await BrowserTestUtils.browserLoaded(browser, undefined, "resource://gre/greprefs.js");

    ok(true, "Got to the end of the test!");
  });
});
