/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test is designed to test responsiveness while performing actions
 * on various context menus in both content and chrome.
 */

Components.utils.import("resource://mozmill/driver/mozmill.js");
let c = getBrowserController();

// Open mozilla.org and wait for the page to load
c.open("http://mozilla.org");
c.waitForPageLoad();

// Perform our first action, reload.
// It is very important to only place things that we
// are interested in testing inside of a performAction call
pep.performAction('content_reload', function() {
  // controller.rootElement is the global window object
  // wrapped inside of a MozMillElement
  c.rootElement.rightClick();
  c.rootElement.keypress('r');
});
c.waitForPageLoad();

c.open("http://mozillians.org");
c.waitForPageLoad();

// Perform our second action, go back
pep.performAction('content_back', function() {
  c.rootElement.rightClick();
  c.rootElement.keypress('b');
});
// Bug 699400 - waitForPageLoad times out when pressing back button
c.sleep(500);

// get a reference to the element with id 'home'
page = findElement.ID(c.tabs.activeTab, 'home');
// Perform our third action, scroll through context menu
pep.performAction('content_scroll', function() {
  page.rightClick();
  for (let i = 0; i < 15; ++i) {
    page.keypress('VK_DOWN');
    // Sleep to better emulate a user
    c.sleep(10);
  }
});

// close the context menu
page.keypress('VK_ESCAPE');

// Now test context menus in chrome
let bar = findElement.ID(c.window.document, "toolbar-menubar");
bar.click();
pep.performAction('chrome_navigation', function() {
  bar.rightClick();
  bar.keypress('n');
  c.sleep(100);
  bar.rightClick();
  bar.keypress('n');
});

pep.performAction('chrome_scroll', function() {
  bar.rightClick();
  for (let i = 0; i < 15; ++i) {
    bar.keypress('VK_DOWN');
    // Sleep to better emulate a user
    c.sleep(10);
  }
});

bar.keypress('VK_ESCAPE');
